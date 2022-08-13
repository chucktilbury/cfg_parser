#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "memory.h"
#include "values.h"

static Value* cfg_store = NULL;

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

const char* getValStr(Value* val, int idx)
{
    assert(val != NULL);
    assert(idx >= 0);
    // TODO: perform string substitutions

    if(idx < val->list.len)
        return (val->list.list[idx]->data.str);
    else {
        fprintf(stderr, "cfg access error: invalid index: %d\n", idx);
        exit(1);
    }
    return NULL; // keep compiler happy
}

double getValFnum(Value* val, int idx)
{
    assert(val != NULL);
    assert(idx >= 0);

    if(idx < val->list.len)
        return (val->list.list[idx]->data.fnum);
    else {
        fprintf(stderr, "cfg access error: invalid index: %d\n", idx);
        exit(1);
    }
    return 0; // keep compiler happy
}

long int getValNum(Value* val, int idx)
{
    assert(val != NULL);
    assert(idx >= 0);

    if(idx < val->list.len)
        return (val->list.list[idx]->data.num);
    else {
        fprintf(stderr, "cfg access error: invalid index: %d\n", idx);
        exit(1);
    }
    return 0; // keep compiler happy
}

unsigned char getValBool(Value* val, int idx)
{
    assert(val != NULL);
    assert(idx >= 0);

    if(idx < val->list.len)
        return (val->list.list[idx]->data.bval);
    else {
        fprintf(stderr, "cfg access error: invalid index: %d\n", idx);
        exit(1);
    }
    return 0; // keep compiler happy
}

const char* readValStr(const char* name, int idx)
{
    assert(name != NULL);
    assert(idx >= 0);

    Value* val = findVal(name);
    if(val != NULL)
        return getValStr(val, idx);
    else {
        fprintf(stderr, "cfg access error: unknown name: %s\n", name);
        exit(1);
    }
    return 0; // keep compiler happy
}

double readValFnum(const char* name, int idx)
{
    assert(name != NULL);
    assert(idx >= 0);

    Value* val = findVal(name);
    if(val != NULL)
        return getValFnum(val, idx);
    else {
        fprintf(stderr, "cfg access error: unknown name: %s\n", name);
        exit(1);
    }
    return 0; // keep compiler happy
}

long int readValNum(const char* name, int idx)
{
    assert(name != NULL);
    assert(idx >= 0);

    Value* val = findVal(name);
    if(val != NULL)
        return getValNum(val, idx);
    else {
        fprintf(stderr, "cfg access error: unknown name: %s\n", name);
        exit(1);
    }
    return 0; // keep compiler happy
}

unsigned char readValBool(const char* name, int idx)
{
    assert(name != NULL);
    assert(idx >= 0);

    Value* val = findVal(name);
    if(val != NULL)
        return getValBool(val, idx);
    else {
        fprintf(stderr, "cfg access error: unknown name: %s\n", name);
        exit(1);
    }
    return 0; // keep compiler happy
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

