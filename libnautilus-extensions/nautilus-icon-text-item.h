/* nautilus-icon-text-item:  an editable text block with word wrapping for the
 * GNOME canvas.
 *
 * Copyright (C) 1998, 1999 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena <federico@gimp.org>
 */

#ifndef NAUTILUS_ICON_TEXT_ITEM_H
#define NAUTILUS_ICON_TEXT_ITEM_H

#include <gtk/gtkeditable.h>
#include <libgnome/gnome-defs.h>
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

	double x_center;	/* center of text, item coords */
	double y_top;		/* top of text, item coords */
	
	int max_text_width;	/* max width of text - canvas coords */

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
	int  (* text_changed)       (NautilusIconTextItem *item);
	void (* text_edited)        (NautilusIconTextItem *item);
	void (* height_changed)     (NautilusIconTextItem *item);
	void (* width_changed)      (NautilusIconTextItem *item);
	void (* editing_started)    (NautilusIconTextItem *item);
	void (* editing_stopped)    (NautilusIconTextItem *item);
	void (* selection_started)  (NautilusIconTextItem *item);
	void (* selection_stopped)  (NautilusIconTextItem *item);
} NautilusIconTextItemClass;

GtkType      nautilus_icon_text_item_get_type              (void);
void         nautilus_icon_text_item_configure             (NautilusIconTextItem *item,
							    double                x_center,
							    double                y_top,
							    int                   max_text_width,
							    GdkFont              *font,
							    const char           *text,
							    gboolean              is_static);
void         nautilus_icon_text_item_setxy                 (NautilusIconTextItem *item,
							    double                x_center,
							    double                y_top);
void         nautilus_icon_text_item_select                (NautilusIconTextItem *item,
							    int                   sel);
void         nautilus_icon_text_item_set_text              (NautilusIconTextItem *item,
							    const char           *text);
const char * nautilus_icon_text_item_get_text              (NautilusIconTextItem *item);
void         nautilus_icon_text_item_start_editing         (NautilusIconTextItem *item);
void         nautilus_icon_text_item_stop_editing          (NautilusIconTextItem *item,
							    gboolean              accept);
GtkEditable *nautilus_icon_text_item_get_renaming_editable (NautilusIconTextItem *item);

#endif /* NAUTILUS_ICON_TEXT_ITEM_H */
