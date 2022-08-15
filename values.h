#ifndef VALUES_H
#define VALUES_H

typedef enum {
    VAL_ERROR,
    VAL_NAME,
    VAL_STR,
    VAL_NUM,
    VAL_FNUM,
    VAL_BOOL,
} ValType;

typedef struct _literal {
    ValType type;
    const char* str;
    union {
        const char* str;
        double fnum;
        long int num;
        unsigned char bval;
    } data;
    struct _literal* prev;
    struct _literal* next;
} Literal;

typedef struct {
    Literal* first;
    Literal* last;
    Literal* index;
} LiteralList;

typedef struct _value {
    const char* name;
    LiteralList* list;
    struct _value* left;
    struct _value* right;
} Value;

Value* createVal(const char* name);
Literal* createLiteral(ValType type, const char* str);

void clearValList(Value* val);

void appendLiteral(Value* val, Literal* lit);
void prependLiteral(Value* val, Literal* lit);
void replaceLiteral(Value* val, Literal* lit, int index);

ValType checkLiteralType(Value* val, int index);
Literal* getLiteral(Value* val, int index);
const char* getLiteralAsStr(Value* val, int index);
long int getLiteralAsNum(Value* val, int index);
double getLiteralAsFnum(Value* val, int index);
unsigned char getLiteralAsBool(Value* val, int index);

Value* findValue(const char* name);
const char* formatStrLiteral(const char* str);

void resetValIndex(Value* val);
Literal* iterateVal(Value* val);
void printLiteralVal(Literal* ve);
const char* literalTypeToStr(ValType type);
const char* literalValToStr(Literal* lit);

#define valueToStr(n, i) literalValToStr(getLiteral(findValue(n), (i)))

#endif
