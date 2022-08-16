#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>

#include "config.h"

// defined in values.c
void dumpValues();

int main(int argc, char** argv)
{
    //cfg_debug = 1;
    if(argc < 2) {
        fprintf(stderr, "USE: %s filename", argv[0]);
        return 1;
    }

    int retv = readConfig(argv[1]);
    Value* val = findValue("description");
    if(val != NULL) {
       Literal* lit = getLiteral(val, 0);
       printLiteralVal(lit);
    }
    if(!getCfgErrors())
        dumpValues();
    else
        printf("test completed with %d errors\n", getCfgErrors());



    // char* str = "name2134.some_numbers.number7";
    // Value* val = findValue(str);
    // Literal* lit = getLiteral(val, 0);
    // printf("value: ");
    // printLiteralVal(lit);
    // printf("\n");
    // // printf("value: %ld\n", getValNum(val, 0));

    // val = findValue("name2134.words");
    // lit = getLiteral(val, 0);
    // printf("value: ");
    // printLiteralVal(lit);
    // printf("\n");
    // // printf("value: %s\n", getValStr(val, 0));

    // val = findValue("name2134.name2.bacon");
    // lit = getLiteral(val, 0);
    // printf("value: ");
    // printLiteralVal(lit);
    // printf("\n");
    // printf("\t-or- %s\n", valueToStr("name2134.name2.bacon", 0));
    // // printf("value: %s\n", getValStr(val, 0));

    // printf("value: %s\n", valueToStr("name2134.some_numbers.Gabba.jubjub", 0));


    return retv;
}