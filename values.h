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

Value* createVal(const char* name);

void clearValList(Value* val);

void appendValStr(Value* val, const char* str);
void appendValFnum(Value* val, double num);
void appendValNum(Value* val, long int num);
void appendValBool(Value* val, unsigned char bval);

void prependValStr(Value* val, const char* str);
void prependValFnum(Value* val, double num);
void prependValNum(Value* val, long int num);
void prependValBool(Value* val, unsigned char bval);

const char* getValStr(Value* val, int idx);
double getValFnum(Value* val, int idx);
long int getValNum(Value* val, int idx);
unsigned char getValBool(Value* val, int idx);

const char* readValStr(const char* val, int idx);
double readValFnum(const char* val, int idx);
long int readValNum(const char* val, int idx);
unsigned char readValBool(const char* val, int idx);

Value* findVal(const char* name);

void resetValIndex(Value* val);
ValEntry* iterateVal(Value* val);
ValEntry* getValEntry(Value* val, int idx);

#define VAL_ENTRY_AS_STR(ve)    ((ve)->data.str)
#define VAL_ENTRY_AS_FNUM(ve)   ((ve)->data.fnum)
#define VAL_ENTRY_AS_NUM(ve)    ((ve)->data.num)
#define VAL_ENTRY_AS_BOOL(ve)   ((ve)->data.bval)

#endif