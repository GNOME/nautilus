/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-font-picker.c - A simple widget to select scalable fonts.

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

#include "nautilus-font-picker.h"

#include "nautilus-gtk-macros.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-string-picker.h"
#include "nautilus-string.h"
#include "nautilus-font-manager.h"

#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkhbox.h>

#include <libgnome/gnome-i18n.h>


typedef struct {
	char *name;
	char *font_file_name;
} FontStyle;

typedef struct {
	char *title;
	char *foundry;
	char *family;
	char *name;
	GList *style_list;
} FontEntry;

static const char *black_listed_fonts[] = {
	"microsoft Webdings",
	"microsoft Wingdings",
	"monotype OCR",
	"URW Zapf Dingbats",
	"URW Symbol",
	"xfree86 cursor"
};

static const gint FONT_PICKER_SPACING = 10;

/* Signals */
typedef enum
{
	CHANGED,
	LAST_SIGNAL
} FontPickerSignals;

struct NautilusFontPickerDetails
{
	NautilusStringPicker *font_name_picker;
	NautilusStringPicker *style_picker;
	GList *font_entry_list;
	GtkWidget *title_label;
};

/* GtkObjectClass methods */
static void nautilus_font_picker_initialize_class (NautilusFontPickerClass *font_picker_class);
static void nautilus_font_picker_initialize       (NautilusFontPicker      *font_picker);
static void nautilus_font_picker_destroy          (GtkObject               *object);
static void font_picker_update                    (NautilusFontPicker      *font_picker);

/* Callbacks */
static void font_name_picker_changed_callback     (GtkWidget               *string_picker,
						   gpointer                 callback_data);
static void style_picker_changed_callback         (GtkWidget               *string_picker,
						   gpointer                 callback_data);
static void font_manager_callback                 (const char              *font_file_name,
						   NautilusFontType         font_type,
						   const char              *foundry,
						   const char              *family,
						   const char              *weight,
						   const char              *slant,
						   const char              *set_width,
						   const char              *char_set_registry,
						   const char              *char_set_encoding,
						   gpointer                 callback_data);
static void font_entry_list_for_each_callback     (gpointer                 data,
						   gpointer                 callback_data);
static void style_list_for_each_callback          (gpointer                 data,
						   gpointer                 callback_data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusFontPicker, nautilus_font_picker, GTK_TYPE_VBOX)

static guint font_picker_signals[LAST_SIGNAL] = { 0 };

/* GtkObjectClass methods */
static void
nautilus_font_picker_initialize_class (NautilusFontPickerClass *font_picker_class)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (font_picker_class);

	/* GtkObjectClass */
	object_class->destroy = nautilus_font_picker_destroy;

	/* Signals */
	font_picker_signals[CHANGED] = gtk_signal_new ("changed",
						       GTK_RUN_LAST,
						       object_class->type,
						       0,
						       gtk_marshal_NONE__NONE,
						       GTK_TYPE_NONE, 
						       0);

	gtk_object_class_add_signals (object_class, font_picker_signals, LAST_SIGNAL);
}

static void
nautilus_font_picker_initialize (NautilusFontPicker *font_picker)
{
	GtkWidget *hbox;

	font_picker->details = g_new0 (NautilusFontPickerDetails, 1);
	gtk_box_set_homogeneous (GTK_BOX (font_picker), FALSE);
	gtk_box_set_spacing (GTK_BOX (font_picker), FONT_PICKER_SPACING);

	font_picker->details->title_label = gtk_label_new ("");
	gtk_label_set_justify (GTK_LABEL (font_picker->details->title_label),
			       GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (font_picker->details->title_label), 0.0, 0.5);

	gtk_box_pack_start (GTK_BOX (font_picker), font_picker->details->title_label, TRUE, TRUE, 0);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (font_picker), hbox, TRUE, TRUE, 0);

	font_picker->details->font_name_picker = NAUTILUS_STRING_PICKER (nautilus_string_picker_new ());
	font_picker->details->style_picker = NAUTILUS_STRING_PICKER (nautilus_string_picker_new ());

	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (font_picker->details->font_name_picker), TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (font_picker->details->style_picker), TRUE, TRUE, 0);

	nautilus_caption_set_title_label (NAUTILUS_CAPTION (font_picker->details->font_name_picker), _("Font"));
 	nautilus_caption_set_show_title (NAUTILUS_CAPTION (font_picker->details->font_name_picker), FALSE);
 	nautilus_caption_set_show_title (NAUTILUS_CAPTION (font_picker->details->style_picker), FALSE);

	gtk_signal_connect (GTK_OBJECT (font_picker->details->font_name_picker),
			    "changed",
			    GTK_SIGNAL_FUNC (font_name_picker_changed_callback),
			    font_picker);

	gtk_signal_connect (GTK_OBJECT (font_picker->details->style_picker),
			    "changed",
			    GTK_SIGNAL_FUNC (style_picker_changed_callback),
			    font_picker);

	gtk_widget_show (GTK_WIDGET (font_picker->details->font_name_picker));
	gtk_widget_show (GTK_WIDGET (font_picker->details->style_picker));
	gtk_widget_show (font_picker->details->title_label);
	gtk_widget_show (hbox);

	/* Populate the font table */
	nautilus_font_manager_for_each_font (font_manager_callback, font_picker);

	/* Populate the font name (foundry+family) picker */
	g_list_foreach (font_picker->details->font_entry_list, font_entry_list_for_each_callback, font_picker);

	/* Update the font picker */
	font_picker_update (font_picker);;
}

/* GtkObjectClass methods */
static void
nautilus_font_picker_destroy (GtkObject* object)
{
	NautilusFontPicker * font_picker;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (object));
	
	font_picker = NAUTILUS_FONT_PICKER (object);

	g_free (font_picker->details);

	/* Chain */
	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static FontEntry *
font_entry_list_find (GList *font_entry_list,
		      const char *font_name)
{
	while (font_entry_list != NULL) {
		FontEntry *entry;
		g_assert (font_entry_list->data != NULL);
		entry = font_entry_list->data;
		if (nautilus_istr_is_equal (font_name, entry->name)) {
			return entry;
		}
		
		font_entry_list = font_entry_list->next;
	}
	
	return NULL;
}

static void
font_picker_update (NautilusFontPicker *font_picker)
{
	char *selected_font;
	const FontEntry *entry;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));
	
	selected_font = nautilus_string_picker_get_selected_string (font_picker->details->font_name_picker);

	entry = font_entry_list_find (font_picker->details->font_entry_list, selected_font);
	g_return_if_fail (entry != NULL);

	nautilus_string_picker_clear (font_picker->details->style_picker);
		
	g_list_foreach (entry->style_list, style_list_for_each_callback, font_picker);
		
	g_free (selected_font);
}

static void
font_name_picker_changed_callback (GtkWidget *string_picker,
				   gpointer callback_data)
{
	NautilusFontPicker *font_picker;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (callback_data));
	g_return_if_fail (NAUTILUS_IS_STRING_PICKER (string_picker));

	font_picker = NAUTILUS_FONT_PICKER (callback_data);

	font_picker_update (font_picker);

	gtk_signal_emit (GTK_OBJECT (font_picker), font_picker_signals[CHANGED]);
}

static void
style_picker_changed_callback (GtkWidget *string_picker, gpointer callback_data)
{
	NautilusFontPicker	*font_picker;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (callback_data));
	g_return_if_fail (NAUTILUS_IS_STRING_PICKER (string_picker));

	font_picker = NAUTILUS_FONT_PICKER (callback_data);

	gtk_signal_emit (GTK_OBJECT (font_picker), font_picker_signals[CHANGED]);
}

static gboolean
ignore_font (const char *font_file_name,
	     const char *foundry,
	     const char *family,
	     const char *entry_name) 
{
	guint i;

	g_return_val_if_fail (font_file_name != NULL, TRUE);
	g_return_val_if_fail (foundry != NULL, TRUE);
	g_return_val_if_fail (family != NULL, TRUE);
	g_return_val_if_fail (entry_name != NULL, TRUE);

	for (i = 0; i < NAUTILUS_N_ELEMENTS (black_listed_fonts); i++) {
		if (nautilus_istr_is_equal (entry_name, black_listed_fonts[i])) {
			return TRUE;
		}
	}

	return FALSE;
}

static void
font_manager_callback (const char *font_file_name,
		       NautilusFontType font_type,
		       const char *foundry,
		       const char *family,
		       const char *weight,
		       const char *slant,
		       const char *set_width,
		       const char *char_set_registry,
		       const char *char_set_encoding,
		       gpointer callback_data)
{
	NautilusFontPicker *font_picker;
	char *entry_name;
	FontEntry *entry;
	FontStyle *style;

	g_return_if_fail (font_file_name != NULL);
	g_return_if_fail (foundry != NULL);
	g_return_if_fail (family != NULL);
	g_return_if_fail (weight != NULL);
	g_return_if_fail (slant != NULL);
	g_return_if_fail (set_width != NULL);
	g_return_if_fail (char_set_registry != NULL);
	g_return_if_fail (char_set_encoding != NULL);
	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (callback_data));

	font_picker = NAUTILUS_FONT_PICKER (callback_data);

	entry_name = g_strdup_printf ("%s %s", foundry, family);

	if (ignore_font (font_file_name, foundry, family, entry_name)) {
		g_free (entry_name);
		return;
	}

	entry = font_entry_list_find (font_picker->details->font_entry_list, entry_name);

	if (entry == NULL) {
		entry = g_new0 (FontEntry, 1);
		entry->name = g_strdup (entry_name);
		entry->foundry = g_strdup (foundry);
		entry->family = g_strdup (family);
		font_picker->details->font_entry_list = g_list_prepend (font_picker->details->font_entry_list, entry);
	}
	g_assert (entry != NULL);
	g_assert (font_entry_list_find (font_picker->details->font_entry_list, entry_name) == entry);
	g_free (entry_name);
	
	style = g_new0 (FontStyle, 1);
	style->name = g_strdup_printf ("%s %s %s", weight, slant, set_width);
	style->font_file_name = g_strdup (font_file_name);

	entry->style_list = g_list_append (entry->style_list, style);
}

static void
style_list_for_each_callback (gpointer data,
			      gpointer callback_data)
{
	NautilusFontPicker *font_picker;
	FontStyle *style;

	g_return_if_fail (data != NULL);
	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (callback_data));

	style = data;
	font_picker = NAUTILUS_FONT_PICKER (callback_data);

	nautilus_string_picker_insert_string (font_picker->details->style_picker, style->name);
}
static void
font_entry_list_for_each_callback (gpointer data,
				   gpointer callback_data)
{
	NautilusFontPicker *font_picker;
	FontEntry *entry;

	g_return_if_fail (data != NULL);
	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (callback_data));

	font_picker = NAUTILUS_FONT_PICKER (callback_data);

 	entry = data;
	nautilus_string_picker_insert_string (font_picker->details->font_name_picker,
					      entry->name);
}

/* NautilusFontPicker public methods */
GtkWidget *
nautilus_font_picker_new (void)
{
	return gtk_widget_new (nautilus_font_picker_get_type (), NULL);
}

char *
nautilus_font_picker_get_selected_font (const NautilusFontPicker *font_picker)
{
	char *result = NULL;
	char *selected_font;
	char *selected_style;
	const FontEntry *entry;
	GList *node;

	g_return_val_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker), NULL);
	
	selected_font = nautilus_string_picker_get_selected_string (font_picker->details->font_name_picker);
	g_return_val_if_fail (selected_font != NULL, NULL);
	
	selected_style = nautilus_string_picker_get_selected_string (font_picker->details->style_picker);
	g_return_val_if_fail (selected_style != NULL, NULL);
	
	entry = font_entry_list_find (font_picker->details->font_entry_list, selected_font);
	g_return_val_if_fail (entry != NULL, NULL);

	node = entry->style_list;
	while (node != NULL && result == NULL) {
		const FontStyle *style;
		g_assert (node->data != NULL);
		style = node->data;

		if (nautilus_istr_is_equal (style->name, selected_style)) {
			result = g_strdup (style->font_file_name);
		}
		node = node->next;
	}
		
	g_free (selected_font);
	g_free (selected_style);

	return result;
}

void
nautilus_font_picker_set_selected_font (NautilusFontPicker *font_picker,
					const char *font_name)
{
	const FontEntry *entry;
	const FontStyle *style;
	GList *font_list_node;
	GList *style_list_node;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));
	
	font_list_node = font_picker->details->font_entry_list;
	while (font_list_node != NULL) {
		g_assert (font_list_node->data != NULL);
		entry = font_list_node->data;

		style_list_node = entry->style_list;
		while (style_list_node != NULL) {
			g_assert (style_list_node->data != NULL);
			style = style_list_node->data;
			
			if (nautilus_istr_is_equal (style->font_file_name, font_name)) {
				nautilus_string_picker_set_selected_string (font_picker->details->font_name_picker,
									    entry->name);
				font_picker_update (font_picker);
				gtk_signal_emit (GTK_OBJECT (font_picker), font_picker_signals[CHANGED]);
				return;
			}
			style_list_node = style_list_node->next;
		}
		
		font_list_node = font_list_node->next;
	}
}

void
nautilus_font_picker_set_title_label (NautilusFontPicker *font_picker,
				      const char *title_label)
{
	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));

	gtk_label_set_text (GTK_LABEL (font_picker->details->title_label), title_label);
}
