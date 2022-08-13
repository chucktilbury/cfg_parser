%{

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "scanner.h"
#include "memory.h"

/*
 * TODO:
 * add if, else, define, and comparison for strings
 */

static char* sec_buf = NULL;
static int sec_cap = 1;
static int sec_len = 0;

static void push_section_name(const char* name)
{
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

static void pop_section_name()
{
    // note that syntax is enforced by the parser
    if(sec_len > 0) {
        char* tmp = strrchr(sec_buf, '.');
        if(tmp != NULL) {
            *tmp = '\0';
            sec_len = strlen(sec_buf);
        }
    }
}

static const char* make_var_name(const char* name)
{
    char* tmp = _alloc(strlen(sec_buf)+strlen(name)+2);
    strcpy(tmp, sec_buf);
    strcat(tmp, ".");
    strcat(tmp, name);

    return tmp;
}

%}

%define parse.error verbose
%locations

%union {
    int num;
    double fnum;
    bool boolval;
    char* name;
    char* value;
    char* qstr;
};

%token INCLUDE
%token <qstr> QSTR
%token <name> NAME
%token <value> VALUE
%token <num> NUM
%token <fnum> FNUM
%token <boolval> TRUE FALSE
%type <boolval> bool_value

%%

module
    : module_list
    ;

module_list
    : module_item
    | module_list module_item
    ;

module_item
    : include
    | section
    ;

include
    : INCLUDE QSTR { printf("(qstr) %p\n", $2); }
    | INCLUDE VALUE { printf("(value) %s\n", $2); }
    | INCLUDE NAME { printf("(name) %s\n", $2); }
    ;

section
    : NAME { push_section_name($1); } '{' section_body '}' { pop_section_name(); }
    ;

bool_value
    : TRUE {}
    | FALSE {}
    ;

section_body
    : section_body_item
    | section_body section_body_item
    ;

value_list_item
    : NAME { printf("\t(name item) %s\n", $1); }
    | VALUE { printf("\t(value item) %s\n", $1); }
    | NUM { printf("\t(number item) %d\n", $1); }
    | FNUM { printf("\t(float item) %f\n", $1); }
    | QSTR { printf("\t(string item) %s\n", $1); }
    | bool_value { printf("\t(bool item) %s", $1? "true": "false"); }
    ;

value_list
    : value_list_item
    | value_list ':' value_list_item
    ;

section_body_item
    : NAME { printf("(item name) %s\n", make_var_name($1)); } '=' value_list
    | section
    | error
    ;

%%

