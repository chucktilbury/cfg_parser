/*
 * This is the (not a)INI library routine. It supports reading an INI
 * from disk and stores it in such a manner as to make it easy to access
 * configuration items, providing that the section name and the entry
 * name are known.
 *
 * Comments start with a '#' and go to the end of the current line.
 * Multi-line values are not supported.
 * All values are stored as strings. No attempt is made to interpret.
 * Nested sections are supported.
 * Macro substitution is supported.
 * White space is ignored.
 * Adding configuration items is supported, such as supporting command
 *  line and environment variables.
 *
 * File format:
 * # This is a comment
 * section_a {
 *      name_1=123
 *      name_2="value"
 *      section_b {
 *          name_1="string with spaces"
 * }}
 *
 * To access the value in section_b, pass a string like
 * "section_a.section_b.name_1" to the getter routine. Likewise, to
 * create an entry in section_b, a string such as
 * "section_a.section_b.name_2=something" to create it. If the same name
 * already exists in the section, then the value is replaced and the old
 * value is discarded.
 *
 * Macro substitution is supported for symbols that have already been
 * defined. To specify a substitute, specify the path of the entry as
 * $(entry).
 *
 * This library routine is not reentrant and there is no need for it to
 * be in the application that it's intended to support.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>

#include "cfg_parser.h"

// in case we are using garbage collection
#ifndef MEMORY_H
#define _alloc(s) malloc(s)
#define _alloc_ds(t) (t*)malloc(sizeof(t))
#define _alloc_ds_array(t, n) (t*)malloc(sizeof(t)*(n))
#define _realloc(p, s) realloc((p), (s))
#define _realloc_ds_array(p, t, n) (t*)realloc((p), sizeof(t)*(n))
#define _copy_str(s) strdup(s)
#define _free(p) free(p)
#endif

typedef enum {
    CFG_NUM,    // all numbers are doubles
    CFG_STR,
    CFG_STR_LST,
    CFG_NUM_LST,
    //CFG_SECTION,
} CfgType;

typedef enum {
    TOK_STR,    // Could be a value or a name
    TOK_NAME,   // name of a var
    TOK_NUM,    // parsed as floating point
    TOK_QSTR,   // quoted string
    TOK_EQU,    // '='
    TOK_COLON,  // ':'
    TOK_OCURL,  // '{'
    TOK_CCURL,  // '}'
    TOK_DQUOT,  // '\"'
    TOK_PLUS,   // '+' context of name or number
    TOK_MINUS,  // '-' context of name or number
    TOK_DOT,    // '.' context of name reference or number (illegal for name)
    TOK_WHT,    // series of W/S characters (mostly ignored)
    TOK_NL,     // new line (mostly ignored)
    TOK_EOF,    // end of file
    TOK_ERR,    // error token; invalid token
} TokenType;

typedef enum {
    CT_INVALID,
    CT_ALPHA,
    CT_NUM,
    CT_PUNCT,
    CT_WS,
} CharType;
#define charTypeTableSize 256

static inline const char* tokTypeToStr(TokenType type)
{
    return (type == TOK_STR)? "STR" :
        (type == TOK_NAME)? "NAME" :
        (type == TOK_NUM)? "NUM" :
        (type == TOK_QSTR)? "QSTR" :
        (type == TOK_EQU)? "EQU" :
        (type == TOK_COLON)? "COLON" :
        (type == TOK_OCURL)? "OCURL" :
        (type == TOK_CCURL)? "CCURL" :
        (type == TOK_DQUOT)? "DQUOT" :
        (type == TOK_PLUS)? "PLUS" :
        (type == TOK_MINUS)? "MINUS" :
        (type == TOK_DOT)? "DOT" :
        (type == TOK_WHT)? "WHT" :
        (type == TOK_NL)? "NL" :
        (type == TOK_EOF)? "EOF" : "UNKNOWN";
}

static const char* nodeTypeToStr(CfgType type)
{
    return (type == CFG_NUM)? "NUM" :
        (type == CFG_STR)? "STR" :
        (type == CFG_STR_LST)? "STR_LST" :
        (type == CFG_NUM_LST)? "NUM_LST" : "UNKNOWN";
}

typedef struct {
    char* buf;
    int cap;
    int len;
} String;

typedef struct {
    String str;
    TokenType type;
} Token;

typedef struct {
    double* list;
    int cap;
    int len;
} NumList;

typedef struct {
    const char* str;
    int cap;
    int len;
} StrList;

typedef struct _cfg_element {
    CfgType type;
    String name;
    union {
        const char* str;
        double num;
        NumList* nlist;
        StrList* slist;
        // a nested section is a child
        struct _cfg_element* child;
    } data;
    struct _cfg_element* left;
    struct _cfg_element* right;
} CfgNode;

// The scanner's data structure.
typedef struct {
    char* buf;
    size_t len;
    size_t head;
    size_t tail;
} FileBuffer;

// static global root node.
static CfgNode* root;
static int line_no = 1;
static CharType char_table[charTypeTableSize];
static String context;

static void syntax(const char* fmt, ...)
{
    va_list args;

    fprintf(stderr, "cfg syntax error: %d: ", line_no);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(1);
}

static inline CharType get_char_type(char ch) {

    return char_table[ch];
}

static void init_string(String* str)
{
    str->cap = 1 << 3;
    str->len = 0;
    str->buf = _alloc_ds_array(char, str->cap);
}

static void add_str_str(String* ptr, const char* str)
{
    size_t len = strlen(str);
    if(ptr->len+len+1 > ptr->cap) {
        while(ptr->cap < ptr->len+len+1)
            ptr->cap <<= 1;
        ptr->buf = _realloc_ds_array(ptr->buf, char, ptr->cap);
    }
    strcpy(&ptr->buf[ptr->len], str);
    ptr->len += len;
}

static void add_str_char(String* ptr, int ch)
{
    if(ptr->len+1 > ptr->cap) {
        ptr->cap <<= 1;
        ptr->buf = _realloc_ds_array(ptr->buf, char, ptr->cap);
    }

    ptr->buf[ptr->len] = (char)ch;
    ptr->len++;
    ptr->buf[ptr->len] = '\0';
}

static FileBuffer* load_file(const char* fname)
{

    FileBuffer* fbuf = _alloc_ds(FileBuffer);

    FILE* fp = fopen(fname, "r");
    if(fp == NULL) {
        fprintf(stderr, "file error: cannot open config file: %s: %s\n", fname, strerror(errno));
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    fclose(fp);

    fbuf->len = len;
    fbuf->buf = _alloc(len+2);
    fbuf->head = fbuf->tail = 0;

    fp = fopen(fname, "r");
    if(fp == NULL) {
        fprintf(stderr, "file error: cannot open config file: %s: %s\n", fname, strerror(errno));
        exit(1);
    }

    fread(fbuf->buf, len, 1, fp);
    fclose(fp);
    fbuf->buf[len] = (char)TOK_EOF;

    return fbuf;
}

static void free_token(Token* tok)
{
#ifndef MEMORY_H
    if(tok != NULL) {
        _free((void*)tok->str.buf);
        _free(tok);
    }
#endif
}

static Token* create_token(const char* str, TokenType type)
{
    Token* tok = _alloc_ds(Token);
    init_string(&tok->str);
    add_str_str(&tok->str, str);
    //tok->str = _copy_str(str);
    tok->type = type;

    return tok;
}

static inline char peek_char(FileBuffer* fb)
{
    if(fb->tail >= fb->len)
        return TOK_EOF;
    else
        return fb->buf[fb->tail];
}

static char read_char(FileBuffer* fb)
{

    if(peek_char(fb) != TOK_EOF) {
        int ch = fb->buf[fb->tail];
        fb->tail++;
        fb->head = fb->tail;
        return ch;
    }
    else
        return (char)TOK_EOF;
}

static void unread_char(FileBuffer* fb)
{
    if(fb->tail > 0) {
        fb->tail--;
        if(fb->tail < fb->head)
            fb->head = fb->tail;
    }
    else
        fb->head = fb->tail = 0;
}

static const char* copy_string(FileBuffer* fb)
{
    size_t len = (fb->tail - fb->head) + 1;
    char *buffer = _alloc(len);

    int i;
    for(i = fb->head; i < fb->tail; i++)
        buffer[i] = fb->buf[i];

    buffer[i] = '\0';
    fb->head = fb->tail;

    return buffer;
}

static void consume_comment(FileBuffer* fb)
{
    int finished = 0;

    while(!finished) {
        int ch = read_char(fb);
        if(ch == '\n') {
            unread_char(fb);
            finished++;
        }
        else if(ch == TOK_EOF)
            finished++;
    }
}

// can only be on the right side of the '='
static void read_string(FileBuffer* fb, Token* tok)
{
    int finished = 0;

    while(!finished) {
        int ch = peek_char(fb);
        if(get_char_type(ch) == CT_ALPHA || get_char_type(ch) == CT_NUM)
            add_str_char(&tok->str, read_char(fb));
        else {
            finished++;
        }
    }
    tok->type = TOK_STR;
}

// can be on the left or the right side of the '='
static void read_name(FileBuffer* fb, Token* tok)
{
    int finished = 0;

    while(!finished) {
        int ch = peek_char(fb);
        if(ch == '$' || ch == '\\') {
            read_string(fb, tok);
            return;
        }
        else if(get_char_type(ch) == CT_ALPHA || get_char_type(ch) == CT_NUM)
            add_str_char(&tok->str, read_char(fb));
        else {
            finished++;
        }
    }
    tok->type = TOK_NAME;
}

// string is in a format where strtod() can parse it.
static void read_number(FileBuffer* fb, Token* tok)
{
    int finished = 0;
    int state = 0;

    while(!finished) {
        int ch = peek_char(fb);
        switch(state) {
            // initial state
            case 0:
                switch(ch) {
                    case '+':
                    case '-':
                        add_str_char(&tok->str, read_char(fb));
                        state = 1; // copy digits
                        break;
                    case '.':
                        add_str_char(&tok->str, '0');
                        add_str_char(&tok->str, '.');
                        read_char(fb);
                        state = 2; // seen a dot
                        break;
                    default:
                        if(isdigit(ch)) {
                            add_str_char(&tok->str, read_char(fb));
                            state = 1; // copy digits
                        }
                        else {
                            syntax("malformed number");
                        }
                }
                break;
            // copy digits state
            case 1:
                switch(ch) {
                    case '.':
                        add_str_char(&tok->str, '.');
                        read_char(fb);
                        state = 2; // seen a dot
                        break;
                    case 'e':
                    case 'E':
                        add_str_char(&tok->str, 'e');
                        read_char(fb);
                        state = 4; // do exponent
                        break;
                    default:
                        if(isdigit(ch))
                            add_str_char(&tok->str, read_char(fb));
                        else {
                            finished++;
                        }
                }
                break;
            // seen a dot, next thing must be a digit
            case 2:
                if(isdigit(ch)) {
                    add_str_char(&tok->str, read_char(fb));
                    state = 3; // copy digits without a dot
                }
                else {
                    syntax("malformed number");
                }
                break;
            // copy digits after the dot
            case 3:
                if(isdigit(ch))
                    add_str_char(&tok->str, read_char(fb));
                else if(ch == 'e' || ch == 'E') {
                    add_str_char(&tok->str, read_char(fb));
                    state = 4;
                }
                else
                    finished++;
                break;
            // exponent
            case 4:
                switch(ch) {
                    case '+':
                    case '-':
                        add_str_char(&tok->str, read_char(fb));
                        state = 5; // only digits are allowed
                        break;
                    default:
                        if(isdigit(ch)) {
                            add_str_char(&tok->str, read_char(fb));
                            state = 5;
                        }
                        else {
                            syntax("malformed number");
                        }
                }
                break;
            // only digits
            case 5:
                if(isdigit(ch))
                    add_str_char(&tok->str, read_char(fb));
                else
                    finished++;
                break;

        }
    }
    tok->type = TOK_NUM;
}

// TODO: Add code for escapes.
static void read_qstrg(FileBuffer* fb, Token* tok)
{
    int finished = 0;

    while(!finished) {
        int ch = peek_char(fb);
        if(ch != '\"') {
            if(ch == '\n') {
                line_no++; // do not save new lines
                read_char(fb); // consume the '\n'
            }
            else if(ch == '\\') {
                // do esc str
                read_char(fb); // consume the back-slash
                switch(peek_char(fb)) {
                    case 'a': read_char(fb); add_str_char(&tok->str, '\a'); break;
                    case 'b': read_char(fb); add_str_char(&tok->str, '\b'); break;
                    case 'e': read_char(fb); add_str_char(&tok->str, '\x1b'); break;
                    case 'f': read_char(fb); add_str_char(&tok->str, '\f'); break;
                    case 'n': read_char(fb); add_str_char(&tok->str, '\n'); break;
                    case 'r': read_char(fb); add_str_char(&tok->str, '\r'); break;
                    case 't': read_char(fb); add_str_char(&tok->str, '\t'); break;
                    case 'v': read_char(fb); add_str_char(&tok->str, '\v'); break;
                    case '\\': read_char(fb); add_str_char(&tok->str, '\\'); break;
                    case '\'': read_char(fb); add_str_char(&tok->str, '\''); break;
                    case '\"': read_char(fb); add_str_char(&tok->str, '\"'); break;
                    case 'x': {
                            // read a 2 digit hex number
                            read_char(fb);  // consume the 'x'
                            char buf[3];

                            int ch = read_char(fb);
                            if(!isxdigit(ch)) {
                                syntax("malformed hex escape");
                            }
                            else
                                buf[0] = ch;

                            ch = read_char(fb);
                            if(!isxdigit(ch)) {
                                syntax("malformed hex escape");
                            }
                            else
                                buf[1] = ch;
                            buf[2] = '\0';

                            add_str_char(&tok->str, (unsigned char)strtol(buf, NULL, 16));
                        }
                        break;
                    default:
                        add_str_char(&tok->str, read_char(fb));
                        break;
                }
            }
            else
                add_str_char(&tok->str, read_char(fb));
        }
        else {
            finished++;
            read_char(fb);  // dump the final dquote
        }
    }
    tok->type = TOK_QSTR;
}

static Token* scan_token(FileBuffer* fb)
{
    Token* tok = _alloc_ds(Token);
    init_string(&tok->str);
    tok->type = TOK_ERR;

    int finished = 0;

    while(!finished) {
        int ch = peek_char(fb);
        switch(ch) {
            case '#':
                consume_comment(fb);
                break;  // and continue
              case '=':
                read_char(fb);
                tok->type = TOK_EQU;
                finished++;
                break;
            case ':':
                read_char(fb);
                tok->type = TOK_COLON;
                finished++;
                break;
            case '{':
                read_char(fb);
                tok->type = TOK_OCURL;
                finished++;
                break;
            case '}':
                read_char(fb);
                tok->type = TOK_CCURL;
                finished++;
                break;
            case '\"':
                read_char(fb);
                tok->type = TOK_DQUOT;
                finished++;
                read_qstrg(fb, tok);
                break;
            case '+':
            case '.':
                read_number(fb, tok);
                finished++;
                break;
            case '-':
                read_char(fb); // consume the '-'
                if(isdigit(peek_char(fb))) {
                    unread_char(fb); // unread the '-'
                    read_number(fb, tok);
                    finished++;
                }
                else {
                    unread_char(fb);
                    read_string(fb, tok); // cannot be a name
                    finished++;
                }
                break;
            case TOK_EOF:
                tok->type = TOK_EOF;
                finished++;
                break;
            default:
                if(ch == '\n') {
                    read_char(fb);
                    line_no++;
                }
                else if(isspace(ch))
                    read_char(fb);  // skip
                else if(isdigit(ch)) {
                    read_number(fb, tok);
                    finished++;
                }
                else {
                    read_name(fb, tok);
                    finished++;
                }
        }
    }
    return tok;
}

static void push_context(const char* str)
{
    add_str_char(&context, '.');
    add_str_str(&context, str);
}

static void pop_context()
{
    if(context.len > 1) {
        char* tmp = strrchr(context.buf, '.');
        if(tmp) {
            *tmp = '\0';
            int len = strlen(context.buf);
            if(len > 0)
                context.len = len;
            else
                add_str_char(&context, '.');
        }
    }
    // silently fail
}

static const char* get_context()
{
    return context.buf;
}

static void add_node(CfgNode* root, CfgNode* node)
{
    int val = strcmp(root->name.buf, node->name.buf);
    if(val > 0) {
        if(root->right != NULL)
            add_node(root->right, node);
        else
            root->right = node;
    }
    else if(val < 0) {
        if(root->left != NULL)
            add_node(root->left, node);
        else
            root->left = node;
    }
    else {
        // overwrite the node value
        root->type = node->type;
        switch(node->type) {
            case CFG_NUM:
                break;
            case CFG_STR:
                break;
            case CFG_STR_LST:
                break;
            case CFG_NUM_LST:
                break;
            default:
                syntax("unknown node type");
        }
    }
}

// TODO: assume it's a list until proven otherwise.
static void parse_value(FileBuffer* fb, const char* name)
{
    CfgNode* node;
    Token* tok = scan_token(fb); // this is the value to store

    if(tok->type == TOK_STR || tok->type == TOK_NAME || tok->type == TOK_QSTR) {
        node = _alloc_ds(CfgNode);
        init_string(&node->name);
        add_str_str(&node->name, name);
        node->type = CFG_STR;
        node->data.str = _copy_str(tok->str.buf);
        free_token(tok);
    }
    else if(tok->type == TOK_NUM) {
        node = _alloc_ds(CfgNode);
        init_string(&node->name);
        add_str_str(&node->name, name);
        node->type = CFG_NUM;
        node->data.num = strtod(tok->str.buf, NULL);
        free_token(tok);
    }
    else {
        syntax("expected a value, but got %s", tok->str.buf);
    }

    if(root == NULL)
        root = node;
    else
        add_node(root, node);
}

// In a section, definitions and sections are allowed.
// incoming token is the name of the section
static void parse_section(FileBuffer* fb, const char* name)
{
    int finished = 0;

    push_context(name);

    // get the body of the section
    while(!finished) {
        Token* tok = scan_token(fb);
        if(tok->type == TOK_NAME) {
            Token* tmp = scan_token(fb);
            if(tmp->type == TOK_EQU) {
                push_context(tok->str.buf);
                parse_value(fb, get_context());
                pop_context();
                free_token(tok);
                free_token(tmp);
            }
            else if(tmp->type == TOK_OCURL) {
                parse_section(fb, tok->str.buf);
                free_token(tok);
                free_token(tmp);
            }
            else {
                syntax("expected a '=' or a '{', but got %s", tok->str.buf);
            }
        }
        else if(tok->type == TOK_CCURL) {
            pop_context();
            free_token(tok);
            finished++;
        }
        else {
            syntax("expected a name or a '}', but got %s", tok->str.buf);
            exit(1);
        }
    }
}

// simple main dispatcher for R/D parser.
// Only things allowed at top level are EOF and SECTIONS
static void parse_input(FileBuffer* fb)
{
    int finished = 0;
    Token* tok;
    Token* tmp;

    while(!finished) {
        tok = scan_token(fb);
        switch(tok->type) {
            case TOK_EOF:
                finished++;
                break;
            case TOK_NAME:
                tmp = scan_token(fb);
                if(tmp->type != TOK_OCURL) {
                    syntax("expected a '{' but got %s", tmp->str.buf);
                    exit(1);
                }
                else {
                    free_token(tmp);
                    parse_section(fb, tok->str.buf);
                }
                break;
            default:
                syntax("unexpected token: %s", tokTypeToStr(tok->type));
                exit(1);
        }
        free_token(tok);
    }
}

FileBuffer* loadCfg(const char* fname)
{
    for(int i = 0; i < charTypeTableSize; i++)
        char_table[i] = CT_INVALID;

    for(int i = 'a'; i <= 'z'; i++)
        char_table[i] = CT_ALPHA;

    for(int i = 'A'; i <= 'Z'; i++)
        char_table[i] = CT_ALPHA;

    for(int i = '0'; i <= '9'; i++)
        char_table[i] = CT_NUM;

    char* tmp = "#{}\":=";
    for(int i = 0; tmp[i] != 0; i++)
        char_table[tmp[i]] = CT_PUNCT;

    tmp = "-+~!@$()%^&*_|\\;<>,?/[].";
    for(int i = 0; tmp[i] != 0; i++)
        char_table[tmp[i]] = CT_ALPHA;

    tmp = " \t\f\r\n";
    for(int i = 0; tmp[i] != 0; i++)
        char_table[tmp[i]] = CT_WS;

    FileBuffer* inf = load_file(fname);

    init_string(&context);

    return inf;
}

const char* getCfgEntry(const char* entry)
{

}

void addCfgEntry(const char* entry)
{

}

static void dump_tree(CfgNode* node)
{
    if(node->right != NULL)
        dump_tree(node->right);
    if(node->left != NULL)
        dump_tree(node->left);

    printf("%s: %s: ", node->name.buf, nodeTypeToStr(node->type));
    switch(node->type) {
        case CFG_NUM: printf("%04f", node->data.num); break;
        case CFG_STR: printf("%s", node->data.str); break;
        default: printf("unsupported"); break;
    }
    printf("\n");
}

int main()
{
    FileBuffer* fb = loadCfg("test.cfg");
    // Token* tok;

    // do {
    //     tok = scan_token(fb);
    //     printf("%-4d%-7s%s\n", line_no, tokTypeToStr(tok->type), tok->str.buf);
    // } while(tok->type != TOK_EOF && tok->type != TOK_ERR);

    printf("\n-------------\n\n%s\n-----------\n", fb->buf);

    parse_input(fb);
    dump_tree(root);
    return 0;
}

