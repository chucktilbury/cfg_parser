
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "memory.h"
#include "parser.h"
#include "scanner.h"
#include "values.h"

void cfg_error(const char *s)
{
    fprintf(stderr, "cfg syntax error: %d: %s at \"%s\"\n", get_line_no(), s, get_text());
}

void dumpValues();

int main()
{
    //cfg_debug = 1;
    push_scanner_file("../test.cfg");
    int retv = cfg_parse();
    dumpValues();

    Value* val = findVal("name2134.some_numbers.number7");
    printf("value: %ld\n", getValNum(val, 0));

    val = findVal("name2134.words");
    printf("value: %s\n", getValStr(val, 0));

    val = findVal("name2134.name2.bacon");
    printf("value: %s\n", getValStr(val, 0));

    return retv;
}