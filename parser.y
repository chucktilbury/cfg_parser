%{

#include "common.h"

#if 0
#define PRN(f, ...) do{printf(f, ## __VA_ARGS__ );}while(0)
#else
#define PRN(f, ...)
#endif

static Value* val;
static char* sec_buf = NULL;
static int sec_cap = 1;
static int sec_len = 0;

typedef struct {
    unsigned char* stack;
    int cap;
    int len;
} StateStack;

StateStack* sstack = NULL;

#define CREATED 0x00
#define INUSE   0x01
#define FINISH  0x02
#define stateToStr(s) ((s) == CREATED? "CREATED": (s) == INUSE? "INUSE": "FINISH")
static int get_state()
{
    if(sstack->len > 0) {
        int state = sstack->stack[sstack->len-1];
        PRN("%d: state: %s\n", get_line_no(), stateToStr(state));
        return state;
    }
    else
        cfgFatalError("invalid parser state");

    return 0;   // keep the compiler happy
}

static void set_state(unsigned char state)
{
    PRN("%d: set state: %s\n", get_line_no(), stateToStr(state));
    if(sstack->len > 0)
        sstack->stack[sstack->len-1] = state;
    else
        cfgFatalError("invalid parser state");
}

static void push_state()
{
    PRN("%d: push state\n", get_line_no());
    if(sstack != NULL) {
        if(sstack->len+1 > sstack->cap) {
            sstack->cap <<= 1;
            sstack->stack = _realloc_ds_array(sstack->stack, unsigned char, sstack->cap);
        }
    }
    else {
        sstack = _alloc_ds(StateStack);
        sstack->len = 0;
        sstack->cap = 1 << 3;
        sstack->stack = _alloc_ds_array(unsigned char, sstack->cap);
    }
    sstack->stack[sstack->len] = CREATED;
    sstack->len++;
}

static void pop_state()
{
    PRN("%d: pop state\n", get_line_no());
    if(sstack->len > 0)
        sstack->len--;
    else
        cfgFatalError("parser state stack under run");
}

static unsigned char eval_expr(Literal* lit)
{
    switch(lit->type) {
        case VAL_ERROR:
        case VAL_NAME:
        case VAL_STR:   return 1;
        case VAL_NUM:   return lit->data.num == 0? 0: 1;
        case VAL_FNUM:  return lit->data.fnum != 0.0? 1: 0;
        case VAL_BOOL:  return lit->data.bval;
        default: cfgFatalError("invalid literal type in eval_expr()");
    }

    return 0; // remove compiler warning.
}

static void push_section_name(const char* name)
{
    if(get_state()) {
        int len = strlen(name);
        if(sec_len+len+2 > sec_cap) {
            while(sec_len+len+2 > sec_cap)
                sec_cap <<= 1;
            sec_buf = _realloc_ds_array(sec_buf, char, sec_cap);
        }
        strcat(sec_buf, ".");
        strcat(sec_buf, name);
        sec_len = strlen(sec_buf);
    }
}

static void pop_section_name()
{
    if(get_state()) {
        // note that syntax is enforced by the parser
        if(sec_len > 0) {
            char* tmp = strrchr(sec_buf, '.');
            if(tmp != NULL) {
                *tmp = '\0';
                sec_len = strlen(sec_buf);
            }
        }
    }
}

static const char* make_var_name(const char* name)
{
    if(sec_buf == NULL) {
        cfg_error("attempt to create a value outside of a section");
        exit(1);
    }

    if(get_state()) {
        char* tmp = _alloc(strlen(sec_buf)+strlen(name)+2);
        strcpy(tmp, sec_buf);
        strcat(tmp, ".");
        strcat(tmp, name);

        return tmp;
    }
    return NULL; // keep the compiler happy
}

static unsigned char comp_vals(Literal* left, Literal* right)
{
    PRN("%d: comp values: (%s)%s == (%s)%s\n",
            get_line_no(), literalTypeToStr(left->type), left->str,
            literalTypeToStr(right->type), right->str);
    switch(left->type) {
        case VAL_ERROR:
            switch(right->type) {
                case VAL_ERROR: return 1;
                case VAL_NAME:  return 0;
                case VAL_STR:   return 0;
                case VAL_NUM:   return 0;
                case VAL_FNUM:  return 0;
                case VAL_BOOL:  return 0;
                default: cfgFatalError("invalid right literal type in comp_nequ()");
            }
            break;
        case VAL_NAME:
            switch(right->type) {
                case VAL_ERROR: return 0;
                case VAL_NAME:
                    /*printf("(%s)%s == (%s)%s\n",
                            literalTypeToStr(left->type), left->data.str,
                            literalTypeToStr(right->type), right->data.str);*/
                    return (strcmp(left->str, right->str) == 0);
                case VAL_STR:
                    /*printf("(%s)%s == (%s)%s\n",
                            literalTypeToStr(left->type), left->data.str,
                            literalTypeToStr(right->type), formatStrLiteral(right->data.str));*/
                    return (strcmp(left->data.str, formatStrLiteral(right->data.str)) == 0);
                case VAL_NUM:   return 0;
                case VAL_FNUM:  return 0;
                case VAL_BOOL:  return 0;
                default: cfgFatalError("invalid right literal type in comp_nequ()");
            }
            break;
        case VAL_STR:
            switch(right->type) {
                case VAL_ERROR: return 0;
                case VAL_NAME:
                    /*printf("(%s)%s == (%s)%s\n",
                            literalTypeToStr(left->type), left->data.str,
                            literalTypeToStr(right->type), right->data.str);*/
                    return (strcmp(formatStrLiteral(left->data.str), right->data.str) == 0);
                case VAL_STR:
                    /*printf("(%s)%s == (%s)%s\n",
                            literalTypeToStr(left->type), formatStrLiteral(left->data.str),
                            literalTypeToStr(right->type), formatStrLiteral(right->data.str));*/
                    return (strcmp(formatStrLiteral(left->data.str), formatStrLiteral(right->data.str)) == 0);
                case VAL_NUM:   return 0;
                case VAL_FNUM:  return 0;
                case VAL_BOOL:  return 0;
                default: cfgFatalError("invalid right literal type in comp_nequ()");
            }
            break;
        case VAL_NUM:
            switch(right->type) {
                case VAL_ERROR: return 0;
                case VAL_NAME:  return 0;
                case VAL_STR:   return 0;
                case VAL_NUM:   return (left->data.num == right->data.num);
                case VAL_FNUM:  return (left->data.num == right->data.fnum);
                case VAL_BOOL:  return (left->data.num == right->data.bval);
                default: cfgFatalError("invalid right literal type in comp_nequ()");
            }
            break;
        case VAL_FNUM:
            switch(right->type) {
                case VAL_ERROR: return 0;
                case VAL_NAME:  return 0;
                case VAL_STR:   return 0;
                case VAL_NUM:   return (left->data.fnum != right->data.num);
                case VAL_FNUM:  return (left->data.fnum != right->data.fnum);
                case VAL_BOOL:  return (left->data.fnum != right->data.bval);
                default: cfgFatalError("invalid right literal type in comp_nequ()");
            }
            break;
        case VAL_BOOL:
            switch(right->type) {
                case VAL_ERROR: return 0;
                case VAL_NAME:  return 0;
                case VAL_STR:   return 0;
                case VAL_NUM:   return (left->data.bval != right->data.num);
                case VAL_FNUM:  return (left->data.bval != right->data.fnum);
                case VAL_BOOL:  return (left->data.bval != right->data.bval);
                default: cfgFatalError("invalid right literal type in comp_nequ()");
            }
            break;
        default: cfgFatalError("invalid left literal type in comp_nequ()");
    }

    return 0;   // can never happen, but the compiler doesn't know it.
}

static Literal* comp_equ(Literal* left, Literal* right)
{
    //printf("%s == %s\n", literalValToStr(left), literalValToStr(right));
    Literal* result = _alloc_ds(Literal);

    result->type = VAL_BOOL;
    result->data.bval = comp_vals(left, right);

    return result;
}

static Literal* comp_nequ(Literal* left, Literal* right)
{
    Literal* result = _alloc_ds(Literal);

    result->type = VAL_BOOL;
    result->data.bval = comp_vals(left, right) == 0? 1: 0;

    return result;
}

static Literal* negate_expr(Literal* val)
{
    Literal* result = _alloc_ds(Literal);

    result->type = VAL_BOOL;
    switch(val->type) {
        case VAL_ERROR:
        case VAL_NAME:
        case VAL_STR: result->data.bval = 0; break;
        case VAL_NUM: result->data.bval = (val->data.num == 0); break;
        case VAL_FNUM: result->data.bval = (val->data.fnum == 0.0); break;
        case VAL_BOOL: result->data.bval = (val->data.bval == 0)? 1: 0; break;
        default: cfgFatalError("invalid literal type in negate_expr()");
    }

    return result;
}

%}

%define parse.error verbose
%locations
%debug

%union {
    Literal* literal;
};

%token INCLUDE IF ELSE EQ NEQ IFDEF IFNDEF NOT DEFINE
%token <literal> QSTR NUM FNUM TRUE FALSE NAME
%type <literal> bool_value value_literal expression

%left EQ NEQ
%left NEGATE

%%

module
    : {
        push_state();
        set_state(INUSE);
    } module_list {
        pop_state();
    }
    ;

module_list
    : module_item
    | module_list module_item
    ;

module_item
    : include_clause
    | section_clause
    | if_clause
    | define_clause
    ;

include_clause
    : INCLUDE QSTR {
            if(get_state() == INUSE)
                push_cfg_file($2->str); // formatting not supported
        }
    | INCLUDE NAME {
            if(get_state() == INUSE)
                push_cfg_file($2->str); // formatting not supported
        }
    ;

value_literal
    : NAME
    | NUM
    | FNUM
    | QSTR
    | bool_value
    ;

value_literal_list
    : value_literal {
            if(get_state() == INUSE)
                appendLiteral(val, $1);
        }
    | value_literal_list ':' value_literal {
            if(get_state() == INUSE)
                appendLiteral(val, $3);
        }
    ;

define_clause
    : DEFINE NAME value_literal {
            if(get_state() == INUSE) {
                //printf("define %s %s\n", $2->str, $3->str);
                appendLiteral(createVal($2->str), $3);
            }
        }
    ;

section_clause
    : NAME '{' {
            if(get_state() == INUSE)
                push_section_name($1->str);
        } section_body '}' {
            if(get_state() == INUSE)
                pop_section_name();
        }
    ;

bool_value
    : TRUE
    | FALSE
    ;

section_body
    : section_body_item
    | section_body section_body_item
    ;

section_body_item
    : NAME '=' {
            if(get_state() == INUSE)
                val = createVal(make_var_name($1->str));
        } value_literal_list
    | section_clause
    | if_clause
    ;

if_body_list
    : section_body_item
    | if_body_list section_body_item
    ;

if_intro
    : IFDEF NAME '{' {
            push_state();
            if(findValue($2->str) != NULL)
                set_state(INUSE);
        }
    | IFNDEF NAME '{' {
            push_state();
            if(findValue($2->str) == NULL)
                set_state(INUSE);
        }
    | IF expression '{' {
            push_state();
            if(eval_expr($2))
                set_state(INUSE);
        }
    ;

if_clause
    : if_intro if_body_list '}' {
            pop_state();
        }
    | if_intro if_body_list '}' {
            if(get_state() == INUSE)
                set_state(FINISH);
        } else_clause_list {
            pop_state();
        }
    ;

else_intro
    : ELSE expression '{' {
            if(get_state() != FINISH) {
                if(eval_expr($2))
                    set_state(INUSE);
            }
        }
    | ELSE '{' {
            if(get_state() != FINISH) {
                set_state(INUSE);
            }
        }
    ;

else_clause
    : else_intro if_body_list '}' {
        if(get_state() == INUSE)
            set_state(FINISH);
    }
    ;

else_clause_list
    : else_clause
    | else_clause_list else_clause
    ;

expression
    : value_literal
    | expression EQ expression { $$ = comp_equ($1, $3); }
    | expression NEQ expression {  $$ = comp_nequ($1, $3); }
    | '(' expression ')' { $$ = $2; }
    | NOT expression %prec NEGATE { $$ = negate_expr($2); }
    ;

%%

