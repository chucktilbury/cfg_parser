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

#if 0
static void add_value_replace(Value* tree, Value* node)
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
        // discard the old values and replace them with the new values.

    }
}

static void add_value_append(Value* tree, Value* node)
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
        // Append the values from the node to the values in the tree

    }
}

static void add_value_prepend(Value* tree, Value* node)
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
        // Prepend the values in the node to the values in the tree.

    }
}
#endif

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

static void add_val_entry(Value* val, ValEntry* ve)
{
    if(val->list.len+1 > val->list.cap) {
        val->list.cap <<= 1;
        val->list.list = _realloc_ds_array(val->list.list, ValEntry*, val->list.cap);
    }
    val->list.list[val->list.len] = ve;
    val->list.len++;
}

Value* createValue(const char* name)
{
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

void addValStr(Value* val, const char* str)
{
    assert(val != NULL);

    ValEntry* ve = _alloc_ds(ValEntry);
    ve->type = VAL_STR;
    ve->data.str = str;

    add_val_entry(val, ve);
}

void addValFnum(Value* val, double num)
{
    assert(val != NULL);

    ValEntry* ve = _alloc_ds(ValEntry);
    ve->type = VAL_FNUM;
    ve->data.fnum = num;

    add_val_entry(val, ve);
}

void addValNum(Value* val, long int num)
{
    assert(val != NULL);

    ValEntry* ve = _alloc_ds(ValEntry);
    ve->type = VAL_NUM;
    ve->data.num = num;

    add_val_entry(val, ve);
}

void addValBool(Value* val, unsigned char bval)
{
    assert(val != NULL);

    ValEntry* ve = _alloc_ds(ValEntry);
    ve->type = VAL_BOOL;
    ve->data.bval = bval;

    add_val_entry(val, ve);
}

Value* findValue(const char* name)
{
    assert(name != NULL);

    if(cfg_store != NULL)
        return find_value(cfg_store, name);
    else
        return NULL;
}

void resetValueIndex(Value* val)
{
    assert(val != NULL);
    val->list.index = 0;
}

ValEntry* iterateValue(Value* val)
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

