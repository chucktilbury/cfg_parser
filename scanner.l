%{

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "memory.h"
#include "parser.h"

void yyerror(const char* s);

static char* buffer = NULL;
static int bcap = 1;
static int blen = 0;

typedef struct _file_stack {
    const char* name;
    int line_no;
    YY_BUFFER_STATE state;
    struct _file_stack* next;
} FileStack;

static FileStack* file_stack = NULL;

void push_scanner_file(const char* fname)
{
    FileStack* fs = _alloc_ds(FileStack);
    fs->name = _copy_str(fname);
    fs->line_no = 1;

    fs->state = YY_CURRENT_BUFFER;

    cfg_in = fopen(fname, "r");
    if(cfg_in == NULL) {
        fprintf(stderr, "cfg error: cannot open input file: %s\n", strerror(errno));
        exit(1);
    }
    yy_switch_to_buffer(yy_create_buffer(cfg_in, YY_BUF_SIZE));

    fs->next = file_stack;
    file_stack = fs;
}

void increment_line_no()
{
    if(file_stack != NULL) {
        file_stack->line_no++;
    }
}

int get_line_no()
{
    if(file_stack != NULL)
        return file_stack->line_no;
    else
        return -1;
}

const char* get_file_name()
{
    if(file_stack != NULL)
        return file_stack->name;
    else
        return "no open file";
}

static void __append_char(char ch)
{
    if(blen+1 >= bcap) {
        bcap <<= 1;
        buffer = _realloc_ds_array(buffer, char, bcap);
    }

    buffer[blen] = ch;
    blen++;
    buffer[blen] = 0;
}

static void __append_str(const char* str)
{
    int len = strlen(str);
    if(blen+len+1 > bcap) {
        while(blen+len+1 > bcap)
            bcap <<= 1;
        buffer = _realloc_ds_array(buffer, char, bcap);
    }

    memcpy(&buffer[blen], str, len+1);
    blen += len;
}

static void __reset_buffer()
{
    blen = 0;
}

const char* get_text()
{
    return cfg_text;
}

%}

%x DQUOTES SQUOTES NAME
%option noinput noyywrap
/* %option verbose debug */

CHARS   [a-zA-Z0-9_\-\$\%\@\!\~\^\&\*\(\)\+\[\]\\\|\;\,\<\>\.\/\?]

%%

\n          { increment_line_no(); }
[ \t\r]     {}

("INCLUDE") { return INCLUDE; }
("TRUE")    { return TRUE; }
("FALSE")   { return FALSE; }
("ON")      { return TRUE; }
("OFF")     { return FALSE; }
"="         { return '='; }
":"         { return ':'; }
"{"         { return '{'; }
"}"         { return '}'; }

[0-9]+ {
        cfg_lval.num = strtol(yytext, NULL, 10);
        return NUM;
    }

    /* recognize a float */
[-+]?([0-9]*\.)?[0-9]+([Ee][-+]?[0-9]+)? {
        cfg_lval.fnum = strtod(yytext, NULL);
        return FNUM;
    }

0[xX][[:xdigit:]]+ {
        cfg_lval.num = strtol(yytext, NULL, 16);
        return NUM;
    }

    /* double quoted strings have escapes managed */
\"  {
        __reset_buffer();
        BEGIN(DQUOTES);
    }

<DQUOTES>\" {
        cfg_lval.qstr = _copy_str(buffer);
        BEGIN(INITIAL);
        return QSTR;
    }

    /* the short rule matches before the long one does */
<DQUOTES>\\n    { __append_char('\n'); }
<DQUOTES>\\r    { __append_char('\r'); }
<DQUOTES>\\e    { __append_char('\x1b'); }
<DQUOTES>\\t    { __append_char('\t'); }
<DQUOTES>\\b    { __append_char('\b'); }
<DQUOTES>\\f    { __append_char('\f'); }
<DQUOTES>\\v    { __append_char('\v'); }
<DQUOTES>\\\\   { __append_char('\\'); }
<DQUOTES>\\\"   { __append_char('\"'); }
<DQUOTES>\\\'   { __append_char('\''); }
<DQUOTES>\\\?   { __append_char('\?'); }
<DQUOTES>\\.    { __append_char(cfg_text[1]); }
<DQUOTES>\\[0-7]{1,3} { __append_char((char)strtol(cfg_text+1, 0, 8));  }
<DQUOTES>\\[xX][0-9a-fA-F]{1,4} { __append_char((char)strtol(cfg_text+2, 0, 16));  }
<DQUOTES>[^\\\"\n]*  { __append_str(cfg_text); }
<DQUOTES>\n     { increment_line_no(); } /* track line numbers, but strip new line */


    /* single quoted strings are absolute literals */
\'  {
        __reset_buffer();
        BEGIN(SQUOTES);
    }

<SQUOTES>\' {
        cfg_lval.qstr = _copy_str(buffer);
        BEGIN(INITIAL);
        return QSTR;
    }

<SQUOTES>[^\\\'\n]*  { __append_str(cfg_text); }
<SQUOTES>\\.    { __append_str(cfg_text); }
<SQUOTES>\n     { __append_str(cfg_text); increment_line_no(); } /* don't strip new lines */

"#".*   { /* do nothing */ }

    /* Scan a name */
{CHARS}+ {
        __reset_buffer();
        __append_str(cfg_text);
        BEGIN(NAME);
    }
<NAME>\\n    { __append_char('\n'); }
<NAME>\\r    { __append_char('\r'); }
<NAME>\\e    { __append_char('\x1b'); }
<NAME>\\t    { __append_char('\t'); }
<NAME>\\b    { __append_char('\b'); }
<NAME>\\f    { __append_char('\f'); }
<NAME>\\v    { __append_char('\v'); }
<NAME>\\\\   { __append_char('\\'); }
<NAME>\\\"   { __append_char('\"'); }
<NAME>\\\'   { __append_char('\''); }
<NAME>\\\?   { __append_char('\?'); }
<NAME>\\[0-7]{1,3} { __append_char((char)strtol(cfg_text+1, 0, 8));  }
<NAME>\\[xX][0-9a-fA-F]{1,4} { __append_char((char)strtol(cfg_text+2, 0, 16));  }
<NAME>[^ \t\r\n\:\=\{\}]+ { __append_str(cfg_text); }
<NAME>\\.       { __append_char(cfg_text[1]); }
<NAME>\n        {
        increment_line_no();
        BEGIN(INITIAL);
        return NAME;
    }
<NAME>[=:{}]    {
        unput(cfg_text[0]);
        cfg_lval.name = _copy_str(buffer);
        BEGIN(INITIAL);
        return NAME;
    }

    /* This pretty much should never happen */
.   {
        fprintf(stderr, "cfg warning: %d: unrecognized character ignored: '%c' (0x%02X)\n",
                    get_line_no(), yytext[0], yytext[0]);
    }

<<EOF>> {
    /* Pop the data structure off of the stack */
    if(file_stack != NULL) {
        FileStack* tmp = file_stack;
        file_stack = tmp->next;
        _free(tmp->name);
        _free(tmp);
    }

    /* If there is an active file, then switch to it. */
    if(file_stack != NULL) {
        yy_delete_buffer(YY_CURRENT_BUFFER);
        yy_switch_to_buffer(file_stack->state);
    }
    else
        yyterminate();
    }


%%