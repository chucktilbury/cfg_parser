#ifndef VALUES_H
#define VALUES_H

typedef enum {
    VAL_STR,
    VAL_NUM,
    VAL_FNUM,
    VAL_BOOL,
} ValType;

typedef struct {
    ValType type;
    union {
        const char* str;
        double fnum;
        long int num;
        unsigned char bval;
    } data;
} ValEntry;

typedef struct {
    ValEntry** list;
    int cap;
    int len;
    int index;
} ValList;

typedef struct _value {
    const char* name;
    ValList list;
    struct _value* left;
    struct _value* right;
} Value;

Value* createValue(const char* name);
void addValStr(Value* val, const char* str);
void addValFnum(Value* val, double num);
void addValNum(Value* val, long int num);
void addValBool(Value* val, unsigned char bval);
Value* findValue(const char* name);
void resetValueIndex(Value* val);
ValEntry* iterateValue(Value* val);

#endif