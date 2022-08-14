#ifndef CFG_COMMON_H
#define CFG_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>

#include "memory.h"
#include "values.h"
#include "parser.h"
#include "scanner.h"

void cfg_fatal_error(const char* fmt, ...);
void cfg_warning(const char* fmt, ...);
#define cfg_syntax cfg_error
void cfg_error(const char *s);
int cfg_get_errors();
int cfg_get_warnings();

#endif