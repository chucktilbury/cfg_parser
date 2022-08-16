#ifndef CMDLINE_H
#define CMDLINE_H

void parseCmdLine(const char* name, int argc, char** argv);
void showCmdLineUse();

int getCmdLineNum(const char* name);
const char* getCmdLineStr(const char* name);
unsigned char getCmdLineBool(const char* name);

#endif