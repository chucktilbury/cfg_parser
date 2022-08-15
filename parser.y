%{

#include "common.h"

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

static void push_state(unsigned char val)
{
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
    sstack->stack[sstack->len] = val;
    sstack->len++;
}

static void pop_state()
{
    if(sstack->len > 0)
        sstack->len--;
    else
        cfg_fatal_error("parser state stack under run");
}

static int get_state()
{
    if(sstack->len > 0)
        return sstack->stack[sstack->len-1];
    else
        cfg_fatal_error("invalid parser state");

    return 0;   // keep the compiler happy
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
        default: cfg_fatal_error("invalid literal type in eval_expr()");
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
    switch(left->type) {
        case VAL_ERROR:
            switch(right->type) {
                case VAL_ERROR: return 1;
                case VAL_NAME:  return 0;
                case VAL_STR:   return 0;
                case VAL_NUM:   return 0;
                case VAL_FNUM:  return 0;
                case VAL_BOOL:  return 0;
                default: cfg_fatal_error("invalid right literal type in comp_nequ()");
            }
            break;
        case VAL_NAME:
            switch(right->type) {
                case VAL_ERROR: return 0;
                case VAL_NAME:  return (strcmp(left->str, right->str) == 0);
                case VAL_STR:   return (strcmp(left->str, formatStrLiteral(right->str)) == 0);
                case VAL_NUM:   return 0;
                case VAL_FNUM:  return 0;
                case VAL_BOOL:  return 0;
                default: cfg_fatal_error("invalid right literal type in comp_nequ()");
            }
            break;
        case VAL_STR:
            switch(right->type) {
                case VAL_ERROR: return 0;
                case VAL_NAME:  return (strcmp(formatStrLiteral(left->str), right->str) == 0);
                case VAL_STR:   return (strcmp(formatStrLiteral(left->str), formatStrLiteral(right->str)) == 0);
                case VAL_NUM:   return 0;
                case VAL_FNUM:  return 0;
                case VAL_BOOL:  return 0;
                default: cfg_fatal_error("invalid right literal type in comp_nequ()");
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
                default: cfg_fatal_error("invalid right literal type in comp_nequ()");
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
                default: cfg_fatal_error("invalid right literal type in comp_nequ()");
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
                default: cfg_fatal_error("invalid right literal type in comp_nequ()");
            }
            break;
        default: cfg_fatal_error("invalid left literal type in comp_nequ()");
    }

    return 0;   // can never happen, but the compiler doesn't know it.
}

static Literal* comp_equ(Literal* left, Literal* right)
{
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
        default: cfg_fatal_error("invalid literal type in negate_expr()");
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
        push_state(1);
    } module_list
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
            if(get_state())
                push_cfg_file($2->str); // formatting not supported
        }
    | INCLUDE NAME {
            if(get_state())
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
    : value_literal { appendLiteral(val, $1); }
    | value_literal_list ':' value_literal { appendLiteral(val, $3); }
    ;

define_clause
    : DEFINE NAME value_literal {
            appendLiteral(createVal($2->str), $3);
        }
    ;

section_clause
    : NAME '{' { push_section_name($1->str); } section_body '}' { pop_section_name(); }
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
            if(get_state())
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
            if(get_state()) {
                Value* val = findValue($2->str);
                if(val != NULL)
                    push_state(1);
                else
                    push_state(0);
            }
        }
    | IFNDEF NAME '{' {
            if(get_state()) {
                Value* val = findValue($2->str);
                if(val != NULL)
                    push_state(0);
                else
                    push_state(1);
            }
        }
    | IF expression '{' {
            if(get_state()) {
                push_state(eval_expr($2));
            }
        }
    ;

if_clause
    : if_intro if_body_list '}' {
            if(get_state()) {
                pop_state();
            }
        }
    | if_intro if_body_list '}' {
            if(get_state()) {
                pop_state();
            }
        } else_clause_list
    ;

else_intro
    : ELSE expression '{' {
            if(get_state()) {
                push_state(eval_expr($2));
            }
        }
    | ELSE '{' {
            if(get_state()) {
                push_state(1);
            }
        }
    ;

else_clause
    : else_intro if_body_list '}' {
            if(get_state()) {
                pop_state();
            }
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

