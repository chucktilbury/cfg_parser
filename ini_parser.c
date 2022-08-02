#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "ini_parser.h"

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
    CFG_BOOL,
    CFG_STR,
    CFG_NUM,
} ConfigType;

typedef enum {
    TOK_SECTION,
    TOK_LABEL,
    TOK_BOOL,
    TOK_STR,
    TOK_NUM,
} TokenType;

typedef enum {
    CT_INVALID  = 0x00, // default type
    CT_WS       = 0x01, // white space
    CT_CR       = 0x02, // a '\n' character
    CT_ALPHA    = 0x04, // 'a' ... 'z' and 'A' ... 'Z'
    CT_NUM      = 0x08, // '0' ... '9'
    CT_XNUM     = 0x10, // '0' ... '9', 'a' ... 'f', and 'A' ... 'F'
    CT_PUNCT    = 0x20, // []=\'":
} CharType;

typedef struct {
    String* str;
    TokenType type;
} Token;

typedef struct {
    char* buf;
    int cap;
    int len;
} String;

typedef struct {
    String** list;
    int cap;
    int len;
} StringList;

typedef struct {
    double* list;
    int cap;
    int len;
} NumberList;

typedef struct {
    bool* list;
    int cap;
    int len;
} BoolList;

typedef struct _value_entry {
    String* name;
    ConfigType type;
    String* val_str;
    union {
        StringList* str;
        NumberList* num;
        BoolList* bval;
    } data;
    struct _value_entry* left;
    struct _value_entry* right;
} ValueEntry;

typedef struct _section_entry {
    String* name;
    ValueEntry* value;
    struct _section_entry* left;
    struct _section_entry* right;
} SectionEntry;

// The scanner's data structure.
typedef struct {
    char* buf;
    size_t len;
    size_t head;
    size_t tail;
    String* fname;
} FileBuf;

static SectionEntry* se_root = NULL;
static SectionEntry* crnt_root = NULL;
static int line_no = 0;

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

static void add_str_str(String* ptr, const char* str)
{
    if(str != NULL) {
        int len = strlen(str);
        if(ptr->len+len+2 > ptr->cap) {
            while(ptr->len+len+2 > ptr->cap)
                ptr->cap <<= 1;
            ptr->buf = _realloc_ds_array(ptr->buf, char, ptr->cap);
        }

        strcat(ptr->buf, str);
        ptr->len += len;
    }
}

static String* create_str(const char* str)
{
    String* ptr = _alloc_ds(String);
    ptr->cap = 0x01 << 3;
    ptr->len == 0;
    ptr->buf = _alloc_ds_array(char, ptr->cap);

    add_str_str(ptr, str);
    return ptr;
}

static void add_se(SectionEntry* tree, SectionEntry* node)
{
    int val = strcmp(tree->name->buf, node->name->buf);
    if(val > 0) {
        if(tree->right != NULL)
            add_se(tree->right, node);
        else
            tree->right = node;
    }
    else if(val < 0) {
        if(tree->left != NULL)
            add_se(tree->left, node);
        else
            tree->left = node;
    }
    else {
        // If the section already exists, then do nothing.
    }
}

static SectionEntry* find_se(SectionEntry* tree, const char* name)
{
    int val = strcmp(tree->name->buf, name);
    if(val > 0) {
        if(tree->right != NULL)
            return find_se(tree->right, name);
        else
            return NULL;
    }
    else if(val < 0) {
        if(tree->left != NULL)
            return find_se(tree->left, name);
        else
            return NULL;
    }
    else
        return tree;
}

static void add_ve(ValueEntry* tree, ValueEntry* node)
{
    int val = strcmp(tree->name->buf, node->name->buf);
    if(val > 0) {
        if(tree->right != NULL)
            add_ve(tree->right, node);
        else
            tree->right = node;
    }
    else if(val < 0) {
        if(tree->left != NULL)
            add_ve(tree->left, node);
        else
            tree->left = node;
    }
    else {
        // replace the entry (name is =)
        tree->type = node->type;
        memcpy(&tree->data, &node->data, sizeof(node->data));
    }
}

static ValueEntry* find_ve(ValueEntry* tree, const char* name)
{
    int val = strcmp(tree->name->buf, name);
    if(val > 0) {
        if(tree->right != NULL)
            return find_ve(tree->right, name);
        else
            return NULL;
    }
    else if(val < 0) {
        if(tree->left != NULL)
            return find_ve(tree->left, name);
        else
            return NULL;
    }
    else
        return tree;
}

static SectionEntry* create_section_entry(const char* name)
{
    SectionEntry* se = _alloc_ds(SectionEntry);
    memset(se, 0, sizeof(SectionEntry));
    se->name = create_str(name);

    if(se_root == NULL)
        se_root = se;
    else
        add_se(se_root, se);

    crnt_root = se;
    return se;
}

static ValueEntry* create_value_entry(const char* section, const char* name, ConfigType type, const char* value)
{
    ValueEntry* node = _alloc_ds(ValueEntry);
    memset(node, 0, sizeof(ValueEntry));
    node->name = create_str(name);
    node->type = type;
    node->val_str = create_str(value);

    SectionEntry* se = find_se(se_root, section);
    if(se == NULL)
        se = create_section_entry(section);

    if(se->value == NULL)
        se->value = node;
    else
        add_ve(se->value, node);
}

static FileBuf* load_file(const char* fname)
{
    FileBuf* fbuf = _alloc_ds(FileBuf);
    memset(fbuf, 0, sizeof(FileBuf));
    fbuf->fname = create_str(fname);

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
    fbuf->buf[len] = (char)EOF;

    return fbuf;
}

static inline char peek_char(FileBuf* fb)
{
    if(fb->tail >= fb->len)
        return EOF;
    else
        return fb->buf[fb->tail];
}

static char read_char(FileBuf* fb)
{

    if(peek_char(fb) != EOF) {
        int ch = fb->buf[fb->tail];
        fb->tail++;
        fb->head = fb->tail;
        if(ch == '\n')
            line_no++;
        return ch;
    }
    else
        return (char)EOF;
}

static char read_char_no_ws(FileBuf* fb)
{

    int finished = 0;

    while(!finished) {
        int ch = read_char(fb);
        if(ch == EOF)
            return EOF;
        else if(ch == '\n')
            line_no++;
        else if(!isspace(ch)) {
            return ch;
        }
        // else loop
    }
}

static void unread_char(FileBuf* fb)
{
    if(fb->tail > 0) {
        fb->tail--;
        if(fb->tail < fb->head)
            fb->head = fb->tail;
    }
    else
        fb->head = fb->tail = 0;
}

static void parse_value(FileBuf* fb)
{

}

static void parse_section(FileBuf* fb)
{
    read_char(fb);  // consume the '['
    String* str = create_str(NULL);
    int finished = 0;
    while(!finished) {
        int ch = read_char(fb);
        if(ch != ']')
            add_str_char(str, ch);
        else
            finished++;
    }
}

static void parse_file(FileBuf* fb)
{
    int finished = 0;
    int ch = peek_char(fb);

    while(!finished) {
        if(ch == EOF)
            finished++;
        else if(ch == '#' || ch == ';')
            consume_comment(fb);
        else if(ch == '[')
            parse_section(fb);
        else
            parse_value(fb);
    }
}

void loadConfig(const char* fname)
{
    FileBuf* fbuf = load_file(fname);
    parse_file(fbuf);
}

bool getCfgBool(const char* section, const char* name)
{
}

const char* getCfgStr(const char* section, const char* name)
{
}

double getCfgNum(const char* section, const char* name)
{
}

void addCfgBool(const char* section, const char* name, bool val)
{
}

void addCfgStr(const char* section, const char* name, const char* val)
{
}

void addCfgNum(const char* section, const char* name, double val)
{
}

