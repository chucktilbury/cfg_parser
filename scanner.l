%{

#include "common.h"

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

void push_cfg_file(const char* fname)
{
    assert(fname != NULL);

    FileStack* fs = _alloc_ds(FileStack);
    fs->name = _copy_str(fname);
    fs->line_no = 1;

    cfg_in = fopen(fname, "r");
    if(cfg_in == NULL)
        cfgFatalError("cfg error: cannot open input file: '%s' %s.", fname, strerror(errno));

    yy_switch_to_buffer(yy_create_buffer(cfg_in, YY_BUF_SIZE));
    fs->state = YY_CURRENT_BUFFER;

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

%x DQUOTES SQUOTES
%option noinput noyywrap nounput
/* %option verbose debug */

%%

"INCLUDE"   { return INCLUDE; }
"IF"        { return IF; }
"ELSE"      { return ELSE; }
"IFDEF"     { return IFDEF; }
"IFNDEF"    { return IFNDEF; }
"EQ"        { return EQ; }
"NEQ"       { return NEQ; }
"NOT"       { return NOT; }
"DEFINE"    { return DEFINE; }
"TRUE"|"ON" {
        cfg_lval.literal = createLiteral(VAL_BOOL, cfg_text);
        cfg_lval.literal->data.bval = 1;
        return TRUE;
    }
"FALSE"|"OFF" {
        cfg_lval.literal = createLiteral(VAL_BOOL, cfg_text);
        cfg_lval.literal->data.bval = 0;
        return FALSE;
    }

"="         { return '='; }
":"         { return ':'; }
"{"         { return '{'; }
"}"         { return '}'; }
"("         { return '('; }
")"         { return ')'; }

[0-9]+ {
        //cfg_lval.num = strtol(yytext, NULL, 10);
        cfg_lval.literal = createLiteral(VAL_NUM, cfg_text);
        cfg_lval.literal->data.num = strtol(yytext, NULL, 10);
        return NUM;
    }

    /* recognize a float */
[-+]?([0-9]*\.)?[0-9]+([Ee][-+]?[0-9]+)? {
        //cfg_lval.fnum = strtod(yytext, NULL);
        cfg_lval.literal = createLiteral(VAL_FNUM, cfg_text);
        cfg_lval.literal->data.fnum = strtod(yytext, NULL);
        return FNUM;
    }

0[xX][[:xdigit:]]+ {
        //cfg_lval.num = strtol(yytext, NULL, 16);
        cfg_lval.literal = createLiteral(VAL_NUM, cfg_text);
        cfg_lval.literal->data.num = strtol(yytext, NULL, 16);
        return NUM;
    }

    /* double quoted strings have escapes managed */
\"  {
        __reset_buffer();
        BEGIN(DQUOTES);
    }

<DQUOTES>\" {
        //cfg_lval.qstr = _copy_str(buffer);
        cfg_lval.literal = createLiteral(VAL_STR, buffer);
        cfg_lval.literal->data.str = cfg_lval.literal->str;
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
        //cfg_lval.qstr = _copy_str(buffer);
        cfg_lval.literal = createLiteral(VAL_STR, buffer);
        cfg_lval.literal->data.str = cfg_lval.literal->str;
        BEGIN(INITIAL);
        return QSTR;
    }

<SQUOTES>[^\\\'\n]*  { __append_str(cfg_text); }
<SQUOTES>\\.    { __append_str(cfg_text); }
<SQUOTES>\n     { __append_str(cfg_text); increment_line_no(); } /* don't strip new lines */

"#".*   { /* do nothing */ }

    /* Scan a name */
[a-zA-Z0-9_$%&\-\.]+ {
        cfg_lval.literal = createLiteral(VAL_NAME, cfg_text);
        cfg_lval.literal->data.str = _copy_str(cfg_text);
        return NAME;
    }

\n          { increment_line_no(); }
[ \t\r]     { /* ignore white space */ }

    /* This pretty much should never happen */
.   {
        cfgWarning("unrecognized character ignored: '%c' (0x%02X)", cfg_text[0], cfg_text[0]);
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

