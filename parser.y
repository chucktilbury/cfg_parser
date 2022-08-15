%{

#include "common.h"

static Value* val;
static char* sec_buf = NULL;
static int sec_cap = 1;
static int sec_len = 0;
static int storage_flag = 1;

static void push_section_name(const char* name)
{
    if(storage_flag) {
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
    if(storage_flag) {
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
    if(storage_flag) {
        char* tmp = _alloc(strlen(sec_buf)+strlen(name)+2);
        strcpy(tmp, sec_buf);
        strcat(tmp, ".");
        strcat(tmp, name);

        return tmp;
    }
    return NULL; // keep the compiler happy
}

%}

%define parse.error verbose
%locations
%debug

%union {
    // int num;
    // double fnum;
    // bool boolval;
    // char* qstr;
    // char* name;

    Literal* literal;
    Value* value;
};

%token INCLUDE IF ELSE EQ NEQ IFDEF IFNDEF NOT DEFINE
%token <literal> QSTR
%token <literal> NUM
%token <literal> FNUM
%token <literal> TRUE FALSE
%token <literal> NAME

%type <literal> bool_value value_literal

%left EQ NEQ
%left NEGATE

%%

module
    : module_list
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
            if(storage_flag)
                push_scanner_file($2->str); // formatting not supported
        }
    | INCLUDE NAME {
            if(storage_flag)
                push_scanner_file($2->str); // formatting not supported
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
            if(storage_flag)
                val = createVal(make_var_name($1->str));
        } value_literal_list
    | section_clause
    | if_clause
    ;

if_body_list
    : section_body_item
    | if_body_list section_body_item
    ;

if_clause
    : IFDEF NAME '{' if_body_list '}' {}
    | IFNDEF NAME '{' if_body_list '}' {}
    | IF expression '{' if_body_list '}' else_clause_list {}
    | IF expression '{' if_body_list '}' {}
    ;

else_clause
    : ELSE expression '{' if_body_list '}'
    | ELSE '{' if_body_list '}'
    ;

else_clause_list
    : else_clause
    | else_clause_list else_clause
    ;

expression
    : value_literal
    | expression EQ expression {}
    | expression NEQ expression {}
    | '(' expression ')'
    | NOT expression %prec NEGATE {}
    ;

%%

