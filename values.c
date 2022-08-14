
#include "common.h"
#include <stdarg.h>

static Value* cfg_store = NULL;

typedef struct {
    char* buf;
    int cap;
    int len;
} str_t;

static void add_str_str(str_t* s, const char* str)
{
    if(str != NULL) {
        int len = strlen(str);
        if(s->len+len+1 > s->cap) {
            while(s->len+len+1 > s->cap)
                s->cap <<= 1;
            s->buf = _realloc_ds_array(s->buf, char, s->cap);
        }
        strcpy(&s->buf[s->len], str);
        s->len += len;
    }
}

static void add_str_char(str_t* s, int ch)
{
    if(s->len+2 > s->cap) {
        s->cap <<= 1;
        s->buf = _realloc_ds_array(s->buf, char, s->cap);
    }
    s->buf[s->len] = (char)ch;
    s->len++;
    s->buf[s->len] = '\0';
}

static void reset_str(str_t* s)
{
    s->len = 0;
    s->buf[0] = '\0';
}

static void add_str_fmt(str_t* s, const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    char* buf = _alloc(len+1);
    va_start(args, fmt);
    vsnprintf(buf, len+1, fmt, args);
    va_end(args);

    add_str_str(s, buf);
    _free(buf);
}

static str_t* create_str(const char* str)
{
    str_t* s = _alloc_ds(str_t);
    s->len = 0;
    s->cap = 1;
    s->buf = _alloc(s->cap);
    s->buf[0] = '\0';

    add_str_str(s, str);
    return s;
}

static void add_value(Value* tree, Value* node)
{
    int x = strcmp(tree->name, node->name);
    if(x > 0) {
        if(tree->right != NULL)
            add_value(tree->right, node);
        else
            tree->right = node;
    }
    else if(x < 0) {
        if(tree->left != NULL)
            add_value(tree->left, node);
        else
            tree->left = node;
    }
    else {
        // This is an error. The value already exists. For now, simply kill the program.
        fprintf(stderr, "cfg warning: node already exists in the configuration table.\n");
        exit(1);
    }
}

static Value* find_value(Value* tree, const char* key)
{
    int x = strcmp(tree->name, key);
    if(x > 0) {
        if(tree->right != NULL)
            return find_value(tree->right, key);
        else
            return NULL;
    }
    else if(x < 0) {
        if(tree->left != NULL)
            return find_value(tree->left, key);
        else
            return NULL;
    }
    else {
        return tree;
    }
}

static void append_val_entry(Value* val, Literal* ve)
{
    LiteralList* list = val->list;
    if(list->last != NULL) {
        ve->prev = list->last;
        list->last->next = ve;
    }
    else
        list->first = ve;

    list->last = ve;
}

static void prepend_val_entry(Value* val, Literal* ve)
{
    LiteralList* list = val->list;
    if(list->first != NULL) {
        ve->next = list->first;
        list->first->prev = ve;
    }
    else
        list->last = ve;
    list->first = ve;
}

static void replace_val_entry(Value* val, Literal* ve, int index)
{
    Literal* tmp = NULL;
    int i;

    resetValIndex(val);
    for(i = 0, tmp = iterateVal(val);
        i < index && tmp != NULL;
        i++, tmp = iterateVal(val)) { /* do nothing here */ }

    if(tmp == NULL)
        append_val_entry(val, ve);
    else {
        ve->next = tmp->next;
        ve->prev = tmp->prev;
        if(ve->next != NULL)
            ve->next->prev = ve;
        if(ve->prev != NULL)
            ve->prev->next = ve;

        if(tmp->type == VAL_QSTR)
            _free(tmp->data.str);
        _free(tmp);

    }
}

/*
 * Implement a (relatively) simple state machine to subs vars into strings.
 *
 * A var is a pre-defined var that is surrounded by $(...). If the var is not
 * found, or an error occurs, then the var is left in the string unchanged.
 */
static const char* do_str_subs(const char* str)
{
    int idx = 0;
    int var_idx = 0;
    int state = 0;
    int finished = 0;
    str_t* s = create_str(NULL);
    str_t* tmp = create_str(NULL);

    while(!finished) {
        switch(state) {
            // initial state, copying characters
            case 0:
                switch(str[idx]) {
                    case '$': state = 1; idx++; break;
                    case '\0': finished++; break;
                    default: add_str_char(s, str[idx++]); break;
                }
                break;
            // seen a '$', check if it's a var opener
            case 1:
                if(str[idx] == '(') {
                    state = 2;
                    idx++;
                }
                else if(str[idx] == '\0') {
                    finished++;
                }
                else {
                    // nope. a '$' by itself is legal.
                    add_str_char(s, '$');
                    add_str_char(s, str[idx++]);
                    state = 1;
                }
                break;
            // seen a var opener, get ready to copy var name
            case 2:
                reset_str(tmp);
                state = 3;
                break;
            // actually copying var name into tmp
            case 3:
                switch(str[idx]) {
                    case '\0':
                        add_str_char(s, '$');
                        add_str_char(s, '(');
                        add_str_str(s, tmp->buf);
                        finished++;
                        break;
                    case ')':
                        // index is required. syntax error
                        fprintf(stderr, "cfg syntax error: %d: index is required near '%s'\n", get_line_no(), str);
                        exit(1);
                        break;
                    case ',':
                        idx++;
                        var_idx = 0;
                        state = 4;
                        break;
                    default:
                        add_str_char(tmp, str[idx++]);
                        break;
                }
                break;
            // index specifier found, read a number
            case 4: {
                    int ch = str[idx];
                    if(isdigit(ch) && ch != ')') {
                        var_idx *= 10;
                        var_idx += (ch - '0');
                        idx++;
                    }
                    else if(ch == ')') {
                        state = 5;
                        idx++;
                    }
                    else {
                        // invalid index. syntax error
                        fprintf(stderr, "cfg syntax error: %d: invalid index near '%s'\n", get_line_no(), str);
                        exit(1);
                    }
                }
                break;
            // var name and index has been found, do the substitution or replace
            // the var in the string
            case 5: {
                    Value* val = findValue(tmp->buf);
                    if(val == NULL)
                        add_str_fmt(s, "$(%s,%d)", tmp->buf, var_idx);
                    else {
                        Literal* ve = getLiteral(val, var_idx);
                        switch(ve->type) {
                            case VAL_QSTR: add_str_fmt(s, "%s", ve->data.str); break;
                            case VAL_NUM: add_str_fmt(s, "%ld", ve->data.num); break;
                            case VAL_FNUM: add_str_fmt(s, "%f", ve->data.fnum); break;
                            case VAL_BOOL: add_str_fmt(s, "%s", ve->data.bval? "TRUE": "FALSE"); break;
                            default:
                                // internal error, should never happen
                                fprintf(stderr, "cfg internal error: unknown value type: %d", ve->type);
                                exit(1);
                                break;
                        }
                    }
                    state = 0;
                }
                break;
        }
    }

    return s->buf;
}

Value* createVal(const char* name)
{
    assert(name != NULL);

    Value* val = _alloc_ds(Value);
    val->name = _copy_str(&name[1]);
    _free(name);
    val->left = NULL;
    val->right = NULL;

    val->list = _alloc_ds(LiteralList);
    val->list->first =
        val->list->last =
        val->list->index = NULL;

    if(cfg_store != NULL)
        add_value(cfg_store, val); // there is nothing in the value yet.
    else
        cfg_store = val;

    return val;
}

Literal* createLiteral(ValType type, const char* str)
{
    Literal* lit = _alloc_ds(Literal);
    lit->type = type;
    lit->str = _copy_str(str);
    switch(type) {
        case VAL_QSTR:  lit->data.str = str; break;
        case VAL_NUM:   lit->data.num = strtol(str, NULL, 10); break;
        case VAL_FNUM:  lit->data.fnum = strtod(str, NULL); break;
        case VAL_BOOL:  lit->data.bval = (strcmp(str, "false"))? 1: 0; break;
        default:        cfg_fatal_error("unknown value type: %d");
    }

    return lit;
}

void clearValList(Value* val)
{
    assert(val != NULL);

    Literal* elem;
    Literal* next;

    for(elem = val->list->first; elem != NULL; elem = next) {
        next = elem->next;
        if(elem->type == VAL_QSTR) {
            _free(elem->data.str);
            elem->data.str = NULL;  // in case GC is in use.
        }
        _free(elem);
        elem = NULL;
    }
}

void appendLiteral(Value* val, Literal* lit) { append_val_entry(val, lit); }
void prependLiteral(Value* val, Literal* lit) { prepend_val_entry(val, lit); }
void replaceLiteral(Value* val, Literal* lit, int index) { replace_val_entry(val, lit, index); }
Literal* getLiteral(Value* val, int index)
{
    Literal* tmp = NULL;
    int i;

    resetValIndex(val);
    for(i = 0, tmp = iterateVal(val);
        i < index && tmp != NULL;
        i++, tmp = iterateVal(val)) { /* do nothing here */ }

    return tmp; // NULL if the index is invalid
}

ValType checkLiteralType(Value* val, int index)
{
    Literal* lit = getLiteral(val, index);
    if(lit != NULL)
        return lit->type;
    else
        return VAL_ERROR;
}

const char* getLiteralAsStr(Value* val, int index)
{
    Literal* lit = getLiteral(val, index);
    if(lit != NULL && lit->type == VAL_QSTR)
        return lit->data.str;
    else
        return lit->str;
}

long int getLiteralAsNum(Value* val, int index)
{
    Literal* lit = getLiteral(val, index);
    if(lit != NULL && lit->type == VAL_NUM)
        return lit->data.num;
    else {
        cfg_warning("attempt to get a %s as a NUM", literalTypeToStr(lit->type));
        return strtol(lit->str, NULL, 10);
    }
}

double getLiteralAsFnum(Value* val, int index)
{
    Literal* lit = getLiteral(val, index);
    if(lit != NULL && lit->type == VAL_FNUM)
        return lit->data.fnum;
    else {
        cfg_warning("attempt to get a %s as a FNUM", literalTypeToStr(lit->type));
        return strtod(lit->str, NULL);
    }
}

unsigned char getLiteralAsBool(Value* val, int index)
{
    Literal* lit = getLiteral(val, index);
    if(lit != NULL && lit->type == VAL_BOOL)
        return lit->data.bval;
    else {
        cfg_warning("attempt to get a %s as a BOOL", literalTypeToStr(lit->type));
        return 0;
    }
}

Value* findValue(const char* name)
{
    assert(name != NULL);

    if(cfg_store != NULL)
        return find_value(cfg_store, name);
    else
        return NULL;
}

void resetValIndex(Value* val)
{
    assert(val != NULL);
    val->list->index = val->list->first;
}

Literal* iterateVal(Value* val)
{
    assert(val != NULL);

    Literal* lit = val->list->index;
    if(lit != NULL)
        val->list->index = lit->next;
    return lit;
}

const char* formatStrLiteral(const char* str)
{
    return do_str_subs(str);
}

void printLiteralVal(Literal* lit)
{
    if(lit != NULL)
        printf("(%s)%s", literalTypeToStr(lit->type), literalValToStr(lit));
    else
        cfg_fatal_error("attempt to print a NULL literal value");
}

const char* literalTypeToStr(ValType type)
{
    return (type == VAL_ERROR)? "ERROR":
            (type == VAL_QSTR)? "QSTR":
            (type == VAL_NUM)? "NUM":
            (type == VAL_FNUM)? "FNUM":
            (type == VAL_BOOL)? "BOOL": "UNKNOWN";
}

const char* literalValToStr(Literal* lit)
{
    char* outstr = NULL;

    switch(lit->type) {
        case VAL_QSTR:
            outstr = (char*)do_str_subs(lit->data.str);
            break;
        case VAL_ERROR:
            outstr = _copy_str("ERROR");
            break;
        case VAL_NUM: {
                int len = snprintf(NULL, 0, "%ld", lit->data.num);
                outstr = _alloc(len+1);
                sprintf(outstr, "%ld", lit->data.num);
            }
            break;
        case VAL_FNUM: {
                int len = snprintf(NULL, 0, "%f", lit->data.fnum);
                outstr = _alloc(len+1);
                sprintf(outstr, "%f", lit->data.fnum);
            }
            break;
        case VAL_BOOL: {
                int len = snprintf(NULL, 0, "%s", lit->data.bval? "true": "false");
                outstr = _alloc(len+1);
                sprintf(outstr, "%s", lit->data.bval? "true": "false");
            }
            break;
        default:
            cfg_fatal_error("unknown literal type: %d", lit->type);
            break;
    }

    return outstr;
}

/*
 * This is not a part of the actual API, but is used for debugging the config parser.
 */
static void dump_values(Value* val)
{
    if(val->right != NULL)
        dump_values(val->right);
    if(val->left != NULL)
        dump_values(val->left);

    printf("\t%s:\n", val->name);
    Literal* ve;
    resetValIndex(val);
    while(NULL != (ve = iterateVal(val))) {
        printf("\t\t");
        printLiteralVal(ve);
        printf("\n");
    }
}

void dumpValues()
{
    printf("\n--------- Dump Values -----------\n");
    if(cfg_store != NULL)
        dump_values(cfg_store);
    else
        printf("\tconfig store is empty\n");
    printf("------- End Dump Values ---------\n");
}

