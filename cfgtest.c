
#include "common.h"
#include <stdarg.h>

static int errors = 0;
static int warnings = 0;

int cfg_get_errors()
{
    return errors;
}

int cfg_get_warnings()
{
    return warnings;
}

void cfg_fatal_error(const char* fmt, ...)
{
    va_list args;

    fprintf(stderr, "cfg fatal error: ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
    exit(1);
}

void cfg_warning(const char* fmt, ...)
{
    va_list args;

    fprintf(stderr, "cfg warning: %d: ", get_line_no());
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
    warnings++;
}

void cfg_error(const char *s)
{
    fprintf(stderr, "cfg syntax error: %d: %s at \"%s\"\n", get_line_no(), s, get_text());
    errors++;
}

// defined in values.c
void dumpValues();

int main()
{
    //cfg_debug = 1;
    push_scanner_file("../test.cfg");
    int retv = cfg_parse();
    dumpValues();

    char* str = "name2134.some_numbers.number7";
    Value* val = findValue(str);
    Literal* lit = getLiteral(val, 0);
    printf("value: ");
    printLiteralVal(lit);
    printf("\n");
    // printf("value: %ld\n", getValNum(val, 0));

    val = findValue("name2134.words");
    lit = getLiteral(val, 0);
    printf("value: ");
    printLiteralVal(lit);
    printf("\n");
    // printf("value: %s\n", getValStr(val, 0));

    val = findValue("name2134.name2.bacon");
    lit = getLiteral(val, 0);
    printf("value: ");
    printLiteralVal(lit);
    printf("\n");
    // printf("value: %s\n", getValStr(val, 0));

    return retv;
}