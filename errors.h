#ifndef CFG_ERRORS_H
#define CFG_ERRORS_H

int getCfgErrors();
int getCfgWarnings();
void cfgFatalError(const char* fmt, ...);
void cfgWarning(const char* fmt, ...);
void cfg_error(const char *s);

#endif