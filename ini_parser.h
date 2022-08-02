#ifndef INI_PARSER_H
#define INI_PARSER_H

void loadConfig(const char* fname);
bool getCfgBool(const char* section, const char* name);
const char* getCfgStr(const char* section, const char* name);
double getCfgNum(const char* section, const char* name);
void addCfgBool(const char* section, const char* name, bool val);
void addCfgStr(const char* section, const char* name, const char* val);
void addCfgNum(const char* section, const char* name, double val);

#endif