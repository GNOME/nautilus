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

#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>

static const gint CAPTION_INVALID = -1;
static const gint CAPTION_SPACING = 10;

struct _NautilusCaptionDetail
{
	GtkWidget		*title_label;
	GtkWidget		*child;
};

/* NautilusCaptionClass methods */
static void      nautilus_caption_initialize_class (NautilusCaptionClass *klass);
static void      nautilus_caption_initialize       (NautilusCaption      *caption);

/* GtkObjectClass methods */
static void      nautilus_caption_destroy          (GtkObject            *object);

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
}

static void
nautilus_caption_initialize (NautilusCaption *caption)
{
	caption->detail = g_new (NautilusCaptionDetail, 1);

	gtk_box_set_homogeneous (GTK_BOX (caption), FALSE);
	gtk_box_set_spacing (GTK_BOX (caption), CAPTION_SPACING);

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
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/*
 * NautilusCaption public methods
 */
GtkWidget*
nautilus_caption_new (void)
{
	NautilusCaption *caption;

	caption = gtk_type_new (nautilus_caption_get_type ());
	
	return GTK_WIDGET (caption);
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
 	g_return_if_fail (caption != NULL);
	g_return_if_fail (NAUTILUS_IS_CAPTION (caption));
	g_return_if_fail (title_label != NULL);

	gtk_label_set_text (GTK_LABEL (caption->detail->title_label), title_label);
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

 	g_return_val_if_fail (caption != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_CAPTION (caption), NULL);

	/* DANGER! DANGER!
	 * 
	 * gtk_label_get () does not strdup the result.
	 */
	gtk_label_get (GTK_LABEL (caption->detail->title_label), &str);

	return str ? g_strdup (str) : NULL;
}

/**
 * nautilus_caption_get_title_label:
 * @caption: A NautilusCaption
 *
 * Returns: A newly allocated copy of the title label.
 */
void
nautilus_caption_set_child (NautilusCaption *caption,
			    GtkWidget	    *child)
{
 	g_return_if_fail (caption != NULL);
	g_return_if_fail (NAUTILUS_IS_CAPTION (caption));
	g_return_if_fail (child != NULL);
	g_return_if_fail (GTK_IS_WIDGET (child));
	g_return_if_fail (caption->detail->child == NULL);

	caption->detail->child = child;

	gtk_box_pack_end (GTK_BOX (caption),
			  caption->detail->child,
			  TRUE,		/* expand */
			  TRUE,		/* fill */
			  0);		/* padding */


	gtk_widget_show (caption->detail->child);

}
