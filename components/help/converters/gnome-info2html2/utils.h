#ifndef UTILS_H
#define UTILS_H

#include "data.h"

void strip_spaces( char *str );
void strip_newlines( char *str );
void strip_dupspaces( char *str );
void convert_newlines_to_spaces( char *str );
char *get_line_from_contents( char *ptr, int size );
char *copy_to_anychar( char *str, char *chr, char **copy  );
NODE *alloc_node( void );
void free_node( NODE * );
void map_spaces_to_underscores( char *str );
void fixup_info_filename( char *file );
char *escape_html_chars( char *str );
#endif
