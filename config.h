/*
 * This is the API header for the libconfig.a
 */
#ifndef CONFIG_H
#define CONFIG_H

#include "values.h"
#include "cmdline.h"
#include "errors.h"

int readConfig(const char* fname);

#endif