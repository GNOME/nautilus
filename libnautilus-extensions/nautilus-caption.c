/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-caption.c - A captioned text widget

   Copyright (C) 1999, 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>

#include "nautilus-caption.h"

#include "nautilus-gtk-macros.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-art-gtk-extensions.h"

#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>

static const gint CAPTION_INVALID = -1;
static const gint CAPTION_SPACING = 10;

struct _NautilusCaptionDetail
{
	GtkWidget *title_label;
	GtkWidget *child;
	gboolean show_title;
	int spacing;
};

/* NautilusCaptionClass methods */
static void      nautilus_caption_initialize_class (NautilusCaptionClass *klass);
static void      nautilus_caption_initialize       (NautilusCaption      *caption);

/* GtkObjectClass methods */
static void nautilus_caption_destroy      (GtkObject       *object);
static void nautilus_font_picker_show_all (GtkWidget       *widget);
static void update_title                  (NautilusCaption *caption);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusCaption, nautilus_caption, GTK_TYPE_HBOX)

/*
 * NautilusCaptionClass methods
 */
static void
nautilus_caption_initialize_class (NautilusCaptionClass *caption_class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (caption_class);
	widget_class = GTK_WIDGET_CLASS (caption_class);

	/* GtkObjectClass */
	object_class->destroy = nautilus_caption_destroy;

	/* GtkWidgetClass */
	widget_class->show_all = nautilus_font_picker_show_all;
}

static void
nautilus_caption_initialize (NautilusCaption *caption)
{
	caption->detail = g_new0 (NautilusCaptionDetail, 1);

	gtk_box_set_homogeneous (GTK_BOX (caption), FALSE);
	gtk_box_set_spacing (GTK_BOX (caption), CAPTION_SPACING);

	caption->detail->show_title = TRUE;
	caption->detail->title_label = gtk_label_new ("Title Label:");
	caption->detail->child = NULL;

	gtk_box_pack_start (GTK_BOX (caption),
			    caption->detail->title_label,
			    FALSE,	/* expand */
			    TRUE,	/* fill */
			    0);		/* padding */

	gtk_widget_show (caption->detail->title_label);
}

/*
 * GtkObjectClass methods
 */
static void
nautilus_caption_destroy (GtkObject* object)
{
	NautilusCaption * caption;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_CAPTION (object));
	
	caption = NAUTILUS_CAPTION (object);

	g_free (caption->detail);

	/* Chain */
	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/* GtkObjectClass methods */
static void
nautilus_font_picker_show_all (GtkWidget *widget)
{
	NAUTILUS_CALL_PARENT (GTK_WIDGET_CLASS, show_all, (widget));

	/* Now update the title visibility */
	update_title (NAUTILUS_CAPTION (widget));
}

static void
update_title (NautilusCaption	*caption)
{
	g_return_if_fail (NAUTILUS_IS_CAPTION (caption));

	if (caption->detail->show_title) {
		gtk_widget_show (caption->detail->title_label);
	}
	else {
		gtk_widget_hide (caption->detail->title_label);
	}
}

/*
 * NautilusCaption public methods
 */
GtkWidget *
nautilus_caption_new (void)
{
	return gtk_widget_new (nautilus_caption_get_type (), NULL);
}

/**
 * nautilus_caption_set_title_label:
 * @caption: A NautilusCaption
 * @title_label: The title label
 *
 */
void
nautilus_caption_set_title_label (NautilusCaption		*caption,
				  const char			*title_label)
{
	g_return_if_fail (NAUTILUS_IS_CAPTION (caption));
	g_return_if_fail (title_label != NULL);

	gtk_label_set_text (GTK_LABEL (caption->detail->title_label), title_label);
}

/**
 * nautilus_caption_set_show_title:
 * @caption: A NautilusCaption
 * @show_title: Whether to show the title or not
 *
 */
void
nautilus_caption_set_show_title (NautilusCaption	*caption,
				 gboolean		show_title)
				 {
	g_return_if_fail (NAUTILUS_IS_CAPTION (caption));

	if (caption->detail->show_title == show_title) {
		return;
	}

	caption->detail->show_title = show_title;

	update_title (caption);
}

/**
 * nautilus_caption_get_title_label:
 * @caption: A NautilusCaption
 *
 * Returns: A newly allocated copy of the title label.
 */
char *
nautilus_caption_get_title_label (const NautilusCaption *caption)
{
	gchar *str;

	g_return_val_if_fail (NAUTILUS_IS_CAPTION (caption), NULL);

	/* DANGER! DANGER!
	 * 
	 * gtk_label_get () does not strdup the result.
	 */
	gtk_label_get (GTK_LABEL (caption->detail->title_label), &str);

	return str ? g_strdup (str) : NULL;
}

/**
 * nautilus_caption_get_title_label_width:
 * @caption: A NautilusCaption
 *
 * Returns: A width of the title label.
 */
int
nautilus_caption_get_title_label_width (const NautilusCaption *caption)
{
	NautilusDimensions title_dimensions;
	
	g_return_val_if_fail (NAUTILUS_IS_CAPTION (caption), 0);
	
	title_dimensions = nautilus_gtk_widget_get_preferred_dimensions (caption->detail->title_label);
	
	return title_dimensions.width;
}

/**
 * nautilus_caption_set_child
 * @caption: A NautilusCaption
 * @child: A GtkWidget to become the caption's one and only child.
 * @expand: Same as GtkBox.
 * @fill: Same as GtkBox.
 *
 * Install a widget as the one and only child of the caption.
 */
void
nautilus_caption_set_child (NautilusCaption *caption,
			    GtkWidget *child,
			    gboolean expand,
			    gboolean fill)
{
	g_return_if_fail (NAUTILUS_IS_CAPTION (caption));
	g_return_if_fail (GTK_IS_WIDGET (child));
	g_return_if_fail (caption->detail->child == NULL);

	caption->detail->child = child;
	
	gtk_box_pack_start (GTK_BOX (caption),
			    caption->detail->child,
			    expand,	/* expand */
			    fill,	/* fill */
			    caption->detail->spacing);	/* padding */
	
	gtk_widget_show (caption->detail->child);
}

/**
 * nautilus_caption_set_spacing
 * @caption: A NautilusCaption
 * @spacing: Spacing in pixels between the title and the child.
 *
 * Set the spacing between the title label and the caption's one
 * and only child.
 */
void
nautilus_caption_set_spacing (NautilusCaption *caption,
			      int spacing)
{
	gboolean expand;
	gboolean fill;
	guint padding;
	GtkPackType pack_type;

	g_return_if_fail (NAUTILUS_IS_CAPTION (caption));
	g_return_if_fail (spacing >= 0);

	caption->detail->spacing = spacing;
	
	if (caption->detail->child == NULL) {
		return;
	}

	gtk_box_query_child_packing (GTK_BOX (caption),
				     caption->detail->child,
				     &expand,
				     &fill,
				     &padding,
				     &pack_type);
	
	gtk_box_set_child_packing (GTK_BOX (caption),
				   caption->detail->child,
				   expand,
				   fill,
				   caption->detail->spacing,
				   pack_type);
}
