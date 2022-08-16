
#include "common.h"

int readConfig(const char* fname)
{
    push_cfg_file(fname);
    return cfg_parse();
}