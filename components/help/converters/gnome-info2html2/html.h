#ifndef HTML_H
#define HTML_H

#define HTML_ROOT "./htmltest"

#define HEADER_SIZE_1 "H1"
#define HEADER_SIZE_2 "H2"

extern char *BaseFilename;
extern char *OverrideBaseFilename;

extern int galeon_mode;

void dump_html_for_node( NODE *node );

void open_body_text_html( FILE *f );
void close_body_text_html( FILE *f );
char *write_body_text_html( FILE *f, char *p, char *q, char *nodefile );

void open_menu_html( FILE *f, char *p );
void close_menu_html( FILE *f );
void write_menu_entry_html( FILE *f, char *p, char *nodefile,char **menu_end );

void write_header_html( FILE *f, char *p, char *hdr );

void write_html_horiz_rule( FILE *f );
#endif
