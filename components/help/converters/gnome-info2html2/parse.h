#ifndef PARSE_H
#define PARSE_H

#include "data.h"

#define READ_OK    1
#define READ_EOF   2
#define READ_ERR   0

NODE *parse_node_line( NODE *node, char * line );
char *parse_node_label( char **line, char *label, int allow_eof );
int parse_menu_line( char *line, char **refname, char **reffile,
                     char **refnode, char **end_of_link,
		     int span_lines);

int parse_note_ref( char *line, char **refname, char **reffile,
                     char **refnode, char **end_of_link,
		     int span_lines);

int read_node_contents( FILE *f, NODE *node );
int read_node (FILE *f, char *line, NODE *node);

int is_a_hdr_line (char *r);

#endif /* PARSE_H  */
