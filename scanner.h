#ifndef SCANNER_H
#define SCANNER_H

int get_line_no();
const char* get_file_name();
void push_scanner_file(const char* fname);
const char* get_text();

/*
 * Defined by flex. Call one time to isolate a symbol and then use the global
 * symbol struct to access the symbol.
 */
extern int cfg_lex(void);
extern int cfg_parse(void);
extern FILE *cfg_in;
extern int cfg_debug;

/*
 * Called by parser for syntax error.
 */
void cfg_syntax(const char *s);

#endif