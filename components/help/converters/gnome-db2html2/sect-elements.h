#ifndef __SECT_ELEMENTS_H__
#define __SECT_ELEMENTS_H__

#include "gdb3html.h"

extern ElementInfo sect_elements[];

gpointer sect_init_data (void);
void sect_article_end_element (Context *context, const gchar *name);

#endif
