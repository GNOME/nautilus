#ifndef __SECT_ELEMENTS_H__
#define __SECT_ELEMENTS_H__

#include "gdb3html.h"

extern ElementInfo sect_elements[];

gpointer sect_init_data (void);

typedef enum SectContextState {
	LOOKING_FOR_SECT,
	LOOKING_FOR_SECT_TITLE,
	IN_SECT,
	LOOKING_FOR_POST_SECT,
	DONE_WITH_SECT
} SectContextState;

typedef struct _SectContext SectContext;
struct _SectContext {
	HeaderInfo *header;
	FigureInfo *figure;
	gint figure_count;
	gchar *prev;
	gchar *previd;
	SectContextState state;
	GHashTable *title_hash;
	/* A list full of GStrings. */
	GList *footnotes;
};

#endif
