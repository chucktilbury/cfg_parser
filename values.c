#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>

#include "scanner.h"
#include "memory.h"
#include "values.h"

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

static void append_val_entry(Value* val, ValEntry* ve)
{
    if(val->list.len+1 > val->list.cap) {
        val->list.cap <<= 1;
        val->list.list = _realloc_ds_array(val->list.list, ValEntry*, val->list.cap);
    }
    val->list.list[val->list.len] = ve;
    val->list.len++;
}

static void prepend_val_entry(Value* val, ValEntry* ve)
{
    if(val->list.len+1 > val->list.cap) {
        val->list.cap <<= 1;
        val->list.list = _realloc_ds_array(val->list.list, ValEntry*, val->list.cap);
    }

    // TODO: fix me
    val->list.list[val->list.len] = ve;
    val->list.len++;
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
                    Value* val = findVal(tmp->buf);
                    if(val == NULL)
                        add_str_fmt(s, "$(%s,%d)", tmp->buf, var_idx);
                    else {
                        ValEntry* ve = getValEntry(val, var_idx);
                        switch(ve->type) {
                            case VAL_STR: add_str_fmt(s, "%s", ve->data.str); break;
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

    val->list.cap = 1;
    val->list.len = 0;
    val->list.index = 0;
    val->list.list = _alloc_ds_array(ValEntry*, val->list.cap);

    if(cfg_store != NULL)
        add_value(cfg_store, val); // there is nothing in the value yet.
    else
        cfg_store = val;

    return val;
}

void clearValList(Value* val)
{
    resetValIndex(val);
    for(int i = 0; i < val->list.len; i++) {
        ValEntry* ve = val->list.list[i];
        if(ve->type == VAL_STR) {
            _free(ve->data.str);
        }
        _free(ve);
        val->list.list[i] = NULL; // in case GC is in use.
    }
    val->list.len = 0;
    // does not change the list capacity
}

void appendValStr(Value* val, const char* str)
{
    assert(val != NULL);
    assert(str != NULL);

    ValEntry* ve = _alloc_ds(ValEntry);
    ve->type = VAL_STR;
    ve->data.str = str;

    append_val_entry(val, ve);
}

void appendValFnum(Value* val, double num)
{
    assert(val != NULL);

    ValEntry* ve = _alloc_ds(ValEntry);
    ve->type = VAL_FNUM;
    ve->data.fnum = num;

    append_val_entry(val, ve);
}

void appendValNum(Value* val, long int num)
{
    assert(val != NULL);

    ValEntry* ve = _alloc_ds(ValEntry);
    ve->type = VAL_NUM;
    ve->data.num = num;

    append_val_entry(val, ve);
}

void appendValBool(Value* val, unsigned char bval)
{
    assert(val != NULL);

    ValEntry* ve = _alloc_ds(ValEntry);
    ve->type = VAL_BOOL;
    ve->data.bval = bval;

    append_val_entry(val, ve);
}

void prependValStr(Value* val, const char* str)
{
    assert(val != NULL);
    assert(str != NULL);

    ValEntry* ve = _alloc_ds(ValEntry);
    ve->type = VAL_STR;
    ve->data.str = str;

    prepend_val_entry(val, ve);
}

void prependValFnum(Value* val, double num)
{
    assert(val != NULL);

    ValEntry* ve = _alloc_ds(ValEntry);
    ve->type = VAL_FNUM;
    ve->data.fnum = num;

    prepend_val_entry(val, ve);
}

void prependValNum(Value* val, long int num)
{
    assert(val != NULL);

    ValEntry* ve = _alloc_ds(ValEntry);
    ve->type = VAL_NUM;
    ve->data.num = num;

    prepend_val_entry(val, ve);
}

void prependValBool(Value* val, unsigned char bval)
{
    assert(val != NULL);

    ValEntry* ve = _alloc_ds(ValEntry);
    ve->type = VAL_BOOL;
    ve->data.bval = bval;

    prepend_val_entry(val, ve);
}

Value* findVal(const char* name)
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
    val->list.index = 0;
}

ValEntry* iterateVal(Value* val)
{
    assert(val != NULL);

    if(val->list.index < val->list.len) {
        ValEntry* ve = val->list.list[val->list.index];
        val->list.index++;
        return ve; // clarity, thank you.
    }
    else
        return NULL;
}

ValEntry* getValEntry(Value* val, int idx)
{
    assert(val != NULL);
    assert(idx >= 0 && idx < val->list.len);

    return val->list.list[idx];
}

const char* getValStr(Value* val, int idx)
{
    assert(val != NULL);
    assert(idx >= 0 && idx < val->list.len);

    return do_str_subs(val->list.list[idx]->data.str);
}

double getValFnum(Value* val, int idx)
{
    assert(val != NULL);
    assert(idx >= 0 && idx < val->list.len);

    return (val->list.list[idx]->data.fnum);
}

long int getValNum(Value* val, int idx)
{
    assert(val != NULL);
    assert(idx >= 0 && idx < val->list.len);

    return (val->list.list[idx]->data.num);
}

unsigned char getValBool(Value* val, int idx)
{
    assert(val != NULL);
    assert(idx >= 0 && idx < val->list.len);

    return (val->list.list[idx]->data.bval);
}

const char* readValStr(const char* name, int idx)
{
    assert(name != NULL);
    assert(idx >= 0);

    Value* val = findVal(name);
    assert(val != NULL);

    return getValStr(val, idx);
}

double readValFnum(const char* name, int idx)
{
    assert(name != NULL);
    assert(idx >= 0);

    Value* val = findVal(name);
    assert(val != NULL);

    return getValFnum(val, idx);
}

long int readValNum(const char* name, int idx)
{
    assert(name != NULL);
    assert(idx >= 0);

    Value* val = findVal(name);
    assert(val != NULL);

    return getValNum(val, idx);
}

unsigned char readValBool(const char* name, int idx)
{
    assert(name != NULL);
    assert(idx >= 0);

    Value* val = findVal(name);
    assert(val != NULL);

    return getValBool(val, idx);
}

/*
 * This is not a part of the actual API, but is used for debugging the config parser.
 */
void printValEntry(ValEntry* ve)
{
    switch(ve->type) {
        case VAL_STR: printf("\t\t(STR)%s\n", ve->data.str); break;
        case VAL_NUM: printf("\t\t(NUM)%ld\n", ve->data.num); break;
        case VAL_FNUM: printf("\t\t(FNUM)%f\n", ve->data.fnum); break;
        case VAL_BOOL: printf("\t\t(BOOL)%s\n", ve->data.bval? "true": "false"); break;
        default: printf("\t\t(UNKNOWN)UNKNOWN (%d)", ve->type); break;
    }
}

static void dump_values(Value* val)
{
    if(val->right != NULL)
        dump_values(val->right);
    if(val->left != NULL)
        dump_values(val->left);

    printf("\t%s:\n", val->name);
    ValEntry* ve;
    resetValIndex(val);
    while(NULL != (ve = iterateVal(val))) {
        printValEntry(ve);
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

