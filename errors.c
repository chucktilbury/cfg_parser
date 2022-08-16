
#include "common.h"
#include <stdarg.h>

static int errors = 0;
static int warnings = 0;

int getCfgErrors()
{
    return errors;
}

int getCfgWarnings()
{
    return warnings;
}

void cfgFatalError(const char* fmt, ...)
{
    va_list args;

    fprintf(stderr, "cfg fatal error: ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
    exit(1);
}

void cfgWarning(const char* fmt, ...)
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
