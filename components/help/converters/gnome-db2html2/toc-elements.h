#ifndef __TOC_ELEMENTS_H__
#define __TOC_ELEMENTS_H__

#include "gdb3html.h"

extern ElementInfo toc_elements[];

gpointer toc_init_data (void);
void     toc_free_data (gpointer data);

#endif
