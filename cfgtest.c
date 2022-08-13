
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "memory.h"
#include "parser.h"
#include "scanner.h"

extern int line_no;

void cfg_error(const char *s)
{
    fprintf(stderr, "cfg syntax error: %d: %s at \"%s\"\n", get_line_no(), s, get_text());
}

int main()
{
    push_scanner_file("../test.cfg");
    return cfg_parse();
}