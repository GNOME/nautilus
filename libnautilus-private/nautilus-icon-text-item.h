/* nautilus-icon-text-item:  an editable text block with word wrapping for the
 * GNOME canvas.
 *
 * Copyright (C) 1998, 1999 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena <federico@gimp.org>
 */

#ifndef _NAUTILUS_ICON_TEXT_ITEM_H_
#define _NAUTILUS_ICON_TEXT_ITEM_H_

#include <libgnome/gnome-defs.h>
#include <gtk/gtkentry.h>
#include <libgnomeui/gnome-canvas.h>
#include <libgnomeui/gnome-icon-text.h>

#define NAUTILUS_ICON_TEXT_ITEM(obj)     (GTK_CHECK_CAST((obj), \
        nautilus_icon_text_item_get_type (), NautilusIconTextItem))
#define NAUTILUS_ICON_TEXT_ITEM_CLASS(k) (GTK_CHECK_CLASS_CAST ((k),\
	nautilus_icon_text_item_get_type ()))
#define NAUTILUS_IS_ICON_TEXT_ITEM(o)    (GTK_CHECK_TYPE((o), \
	nautilus_icon_text_item_get_type ()))

/* This structure has been converted to use public and private parts. */
typedef struct {
	GnomeCanvasItem canvas_item;

	/* Size and maximum allowed width */
	int x, y;
	int width;

	/* Font */
	GdkFont *font;

	/* Private data */
	gpointer priv; /* was GtkEntry *entry */

	/* Actual text */
	char *text;

	/* Text layout information */
	GnomeIconTextInfo *ti;

	/* Whether the text is being edited */
	unsigned int editing : 1;

	/* Whether the text item is selected */
	unsigned int selected : 1;

	/* Whether the user is select-dragging a block of text */
	unsigned int selecting : 1;

	/* Whether the text is allocated by us (FALSE if allocated by the client) */
	unsigned int is_text_allocated : 1;
} NautilusIconTextItem;

typedef struct {
	GnomeCanvasItemClass parent_class;

	/* Signals we emit */
	int  (* text_changed)      (NautilusIconTextItem *iti);
	void (* height_changed)    (NautilusIconTextItem *iti);
	void (* width_changed)     (NautilusIconTextItem *iti);
	void (* editing_started)   (NautilusIconTextItem *iti);
	void (* editing_stopped)   (NautilusIconTextItem *iti);
	void (* selection_started) (NautilusIconTextItem *iti);
	void (* selection_stopped) (NautilusIconTextItem *iti);
} NautilusIconTextItemClass;

GtkType  nautilus_icon_text_item_get_type      (void);

void     nautilus_icon_text_item_configure     (NautilusIconTextItem *iti,
					     int                x,
					     int                y,
					     int                width,
					     GdkFont            *font,
					     const char        *text,
						 gboolean           is_static);

void     nautilus_icon_text_item_setxy         (NautilusIconTextItem *iti,
					     int                x,
					     int                y);

void     nautilus_icon_text_item_select        (NautilusIconTextItem *iti,
					     int                sel);

void	 nautilus_icon_text_item_set_text 		(NautilusIconTextItem *iti, 
											const char *text);


char    *nautilus_icon_text_item_get_text      (NautilusIconTextItem *iti);

void     nautilus_icon_text_item_start_editing (NautilusIconTextItem *iti);
void     nautilus_icon_text_item_stop_editing  (NautilusIconTextItem *iti,
					     gboolean           accept);

#endif /* _NAUTILUS_ICON_TEXT_ITEM_H_ */

