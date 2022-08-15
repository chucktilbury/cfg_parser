#ifndef CFG_ERRORS_H
#define CFG_ERRORS_H

int cfg_get_errors();
int cfg_get_warnings();
void cfg_fatal_error(const char* fmt, ...);
void cfg_warning(const char* fmt, ...);
void cfg_error(const char *s);

#endif