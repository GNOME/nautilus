/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-text-caption.c - A text caption widget.

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

#include "nautilus-text-caption.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-entry.h"

#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtksignal.h>

#include <libgnomevfs/gnome-vfs-utils.h>

#include <string.h>
 
static const gint TEXT_CAPTION_INVALID = -1;
static const gint TEXT_CAPTION_SPACING = 10;

/* Signals */
typedef enum
{
	CHANGED,
	LAST_SIGNAL
} NautilusTextCaptionSignals;

struct _NautilusTextCaptionDetail
{
	GtkWidget		*text;

	gboolean                expand_tilde;
};

/* NautilusTextCaptionClass methods */
static void      nautilus_text_caption_initialize_class (NautilusTextCaptionClass *klass);
static void      nautilus_text_caption_initialize       (NautilusTextCaption      *text_caption);

/* GtkObjectClass methods */
static void      nautilus_text_caption_destroy          (GtkObject                 *object);

/* Editable (entry) callbacks */
static void      entry_changed_callback                  (GtkWidget                 *entry,
							  gpointer                   user_data);
static void      entry_key_press_callback                (GtkWidget                 *widget,
							  GdkEventKey               *event,
							  gpointer                  user_data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusTextCaption, nautilus_text_caption, NAUTILUS_TYPE_CAPTION)

static guint text_caption_signals[LAST_SIGNAL] = { 0 };

/*
 * NautilusTextCaptionClass methods
 */
static void
nautilus_text_caption_initialize_class (NautilusTextCaptionClass *text_caption_class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (text_caption_class);
	widget_class = GTK_WIDGET_CLASS (text_caption_class);

	/* GtkObjectClass */
	object_class->destroy = nautilus_text_caption_destroy;
	
	/* Signals */
	text_caption_signals[CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				object_class->type,
				0,
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 
				0);

	gtk_object_class_add_signals (object_class, text_caption_signals, LAST_SIGNAL);
}

static void
nautilus_text_caption_initialize (NautilusTextCaption *text_caption)
{
	text_caption->detail = g_new (NautilusTextCaptionDetail, 1);

	gtk_box_set_homogeneous (GTK_BOX (text_caption), FALSE);
	gtk_box_set_spacing (GTK_BOX (text_caption), TEXT_CAPTION_SPACING);

	text_caption->detail->text = gtk_entry_new ();
	
	gtk_entry_set_editable (GTK_ENTRY (text_caption->detail->text), TRUE);
	
	nautilus_caption_set_child (NAUTILUS_CAPTION (text_caption),
				    text_caption->detail->text,
				    TRUE,
				    TRUE);

	gtk_signal_connect (GTK_OBJECT (text_caption->detail->text),
			    "changed",
			    GTK_SIGNAL_FUNC (entry_changed_callback),
			    (gpointer) text_caption);
	gtk_signal_connect_after (GTK_OBJECT (text_caption->detail->text),
				  "key_press_event",
				  GTK_SIGNAL_FUNC (entry_key_press_callback),
				  (gpointer) text_caption);

	gtk_widget_show (text_caption->detail->text);
}

/*
 * GtkObjectClass methods
 */
static void
nautilus_text_caption_destroy (GtkObject* object)
{
	NautilusTextCaption * text_caption;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_TEXT_CAPTION (object));
	
	text_caption = NAUTILUS_TEXT_CAPTION (object);

	g_free (text_caption->detail);

	/* Chain */
	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/*
 * Editable (entry) callbacks
 */
static void
entry_changed_callback (GtkWidget *entry, gpointer user_data)
{
	NautilusTextCaption *text_caption;

	g_assert (user_data != NULL);
	g_assert (NAUTILUS_IS_TEXT_CAPTION (user_data));

	text_caption = NAUTILUS_TEXT_CAPTION (user_data);

	gtk_signal_emit (GTK_OBJECT (text_caption), text_caption_signals[CHANGED]);
}

static void
entry_key_press_callback (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	NautilusTextCaption *text_caption;
	char *expanded_text;
	
	text_caption = NAUTILUS_TEXT_CAPTION (user_data);
	
	if (event->keyval == GDK_asciitilde) {
		if (text_caption->detail->expand_tilde) {
			expanded_text = gnome_vfs_expand_initial_tilde (gtk_entry_get_text (GTK_ENTRY (widget)));
			
			gtk_entry_set_text (GTK_ENTRY (widget), expanded_text);
			g_free (expanded_text);
		}
	}
}

/*
 * NautilusTextCaption public methods
 */
GtkWidget *
nautilus_text_caption_new (void)
{
	return gtk_widget_new (nautilus_text_caption_get_type (), NULL);
}

/**
 * nautilus_text_caption_get_text
 * @text_caption: A NautilusTextCaption
 *
 * Returns: A copy of the currently selected text.  Need to g_free() it.
 */
char *
nautilus_text_caption_get_text (const NautilusTextCaption *text_caption)
{
	const char *entry_text;

 	g_return_val_if_fail (text_caption != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_TEXT_CAPTION (text_caption), NULL);

	/* WATCHOUT: 
	 * As of gtk1.2, gtk_entry_get_text() returns a non const reference to
	 * the internal string.
	 */
	entry_text = (const char *) gtk_entry_get_text (GTK_ENTRY (text_caption->detail->text));

	return g_strdup (entry_text);
}

void
nautilus_text_caption_set_text (NautilusTextCaption	*text_caption,
				 const char		*text)
{
 	g_return_if_fail (text_caption != NULL);
	g_return_if_fail (NAUTILUS_IS_TEXT_CAPTION (text_caption));

	gtk_entry_set_text (GTK_ENTRY (text_caption->detail->text), text);
}

void
nautilus_text_caption_set_editable (NautilusTextCaption *text_caption,
				    gboolean editable)
{
	g_return_if_fail (NAUTILUS_IS_TEXT_CAPTION (text_caption));

	gtk_entry_set_editable (GTK_ENTRY (text_caption->detail->text), editable);
}

void
nautilus_text_caption_set_expand_tilde (NautilusTextCaption *text_caption,
					gboolean expand_tilde)
{
	g_return_if_fail (NAUTILUS_IS_TEXT_CAPTION (text_caption));

	text_caption->detail->expand_tilde = expand_tilde;
}
