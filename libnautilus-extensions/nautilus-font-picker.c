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

#include <libgnome/gnome-i18n.h>

#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkhbox.h>

#include <libgnome/gnome-i18n.h>


typedef struct {
	char *name;
	char *font_file_name;
} FontStyleEntry;

typedef struct {
	char *name;
	char *family;
	char *name_for_display;
	GList *style_list;
} FontEntry;

/* These font families are black listed, because they 
 * arent useful at all to display "normal" text - at 
 * least in the context of Nautilus.
 */
static const char *black_listed_font_families[] = {
	"Webdings",
	"Wingdings",
	"OCR",
	"Zapf Dingbats",
	"Symbol",
	"cursor",

	"mincho",
	"gothic"
};

static const char *black_listed_font_foundries[] = {
 	"greek",
 	"grinet",
	/* Abisource fonts are black listed because they
	 * appear to simply be copies of the URW fonts,
	 * and listing them would waste valuable font picker
	 * space for no purpose.
	 */
	"Abisource"
};

typedef struct
{
	const char *name;
	const char *mapped;
} StyleMap;

static const char *normal_style = N_("Normal");
static const char *unknown_style = N_("Unknown");

static const StyleMap font_weight_map[] = {
       	{ "",	      NULL },
       	{ "medium",   NULL },
	{ "regular",  NULL },
	{ "bold",     N_("Bold") },
	{ "book",     N_("Book") },
	{ "black",    N_("Black") },
	{ "demibold", N_("Demibold") },
	{ "light",    N_("Light") }
};

static const StyleMap font_slant_map[] = {
       	{ "",	      NULL },
	{ "r",	      NULL },
	{ "i",	      N_("Italic"), },
	{ "o",	      N_("Oblique"), },
	{ "ri",	      N_("Reverse Italic"), },
	{ "ro",	      N_("Reverse Oblique"), },
	{ "ot",	      N_("Other"), }
};

static const StyleMap font_set_width_map[] = {
       	{ "",                 NULL },
	{ "normal",	      NULL },
	{ "condensed",	      N_("Condensed"), },
	{ "semicondensed",    N_("Semicondensed"), }
};

static const gint FONT_PICKER_SPACING = 10;

static GList *global_font_list = NULL;

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
	GtkWidget *title_label;
};

/* GtkObjectClass methods */
static void         nautilus_font_picker_initialize_class (NautilusFontPickerClass *font_picker_class);
static void         nautilus_font_picker_initialize       (NautilusFontPicker      *font_picker);
static void         nautilus_font_picker_destroy          (GtkObject               *object);

/* Private methods */
static void         font_picker_update_styles             (NautilusFontPicker      *font_picker);
static void         font_picker_populate                  (NautilusFontPicker      *font_picker);

/* Callbacks */
static void         font_picker_font_changed_callback     (GtkWidget               *string_picker,
							   gpointer                 callback_data);
static void         font_picker_style_changed_callback    (GtkWidget               *string_picker,
							   gpointer                 callback_data);

/* Global font list */
static const GList *global_font_list_get                  (void);
static void         global_font_list_free                 (void);
static gboolean     global_font_list_populate_callback    (const char              *font_file_name,
							   NautilusFontType         font_type,
							   const char              *foundry,
							   const char              *family,
							   const char              *weight,
							   const char              *slant,
							   const char              *set_width,
							   const char              *char_set_registry,
							   const char              *char_set_encoding,
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
			    GTK_SIGNAL_FUNC (font_picker_font_changed_callback),
			    font_picker);

	gtk_signal_connect (GTK_OBJECT (font_picker->details->style_picker),
			    "changed",
			    GTK_SIGNAL_FUNC (font_picker_style_changed_callback),
			    font_picker);

	gtk_widget_show (GTK_WIDGET (font_picker->details->font_name_picker));
	gtk_widget_show (GTK_WIDGET (font_picker->details->style_picker));
	gtk_widget_show (font_picker->details->title_label);
	gtk_widget_show (hbox);

	font_picker_populate (font_picker);
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
font_list_find (const GList *font_list,
		const char *font_name)
{
	const GList *node;
	FontEntry *entry;

	/* First check the foundry qualified font names */
	for (node = font_list; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		entry = node->data;

		if (nautilus_istr_is_equal (font_name, entry->name)) {
			return entry;
		}
	}

	/* First check the foundry qualified font names */
	for (node = font_list; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		entry = node->data;

		if (nautilus_istr_is_equal (font_name, entry->family)) {
			return entry;
		}
	}

	return NULL;
}

static void
font_picker_update_styles (NautilusFontPicker *font_picker)
{
	const GList *font_list;
	char *selected_font;
	const FontEntry *font_entry;
	const FontStyleEntry *style_entry;
	const GList *node;
	
	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));
	
	selected_font = nautilus_string_picker_get_selected_string (font_picker->details->font_name_picker);

	font_list = global_font_list_get ();
	g_assert (font_list != NULL);

	font_entry = font_list_find (font_list, selected_font);
	g_return_if_fail (font_entry != NULL);

	nautilus_string_picker_clear (font_picker->details->style_picker);

	for (node = font_entry->style_list; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		style_entry = node->data;
		nautilus_string_picker_insert_string (font_picker->details->style_picker,
						      style_entry->name);
	}
	
	g_free (selected_font);
}

static void
font_picker_populate (NautilusFontPicker *font_picker)
{
 	const GList *font_list;
 	const GList *node;
	const FontEntry *font_entry;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));

	font_list = global_font_list_get ();
	g_assert (font_list != NULL);

	nautilus_string_picker_clear (font_picker->details->font_name_picker);

	for (node = font_list; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		font_entry = node->data;

		nautilus_string_picker_insert_string (font_picker->details->font_name_picker,
						      font_entry->name_for_display);
	}
	
	/* Update the styles picker */
	font_picker_update_styles (font_picker);;
}

/* Callbacks */
static void
font_picker_font_changed_callback (GtkWidget *string_picker,
				   gpointer callback_data)
{
	NautilusFontPicker *font_picker;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (callback_data));
	g_return_if_fail (NAUTILUS_IS_STRING_PICKER (string_picker));

	font_picker = NAUTILUS_FONT_PICKER (callback_data);

	font_picker_update_styles (font_picker);

	gtk_signal_emit (GTK_OBJECT (font_picker), font_picker_signals[CHANGED]);
}

static void
font_picker_style_changed_callback (GtkWidget *string_picker, gpointer callback_data)
{
	NautilusFontPicker *font_picker;
	
	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (callback_data));
	g_return_if_fail (NAUTILUS_IS_STRING_PICKER (string_picker));

	font_picker = NAUTILUS_FONT_PICKER (callback_data);

	gtk_signal_emit (GTK_OBJECT (font_picker), font_picker_signals[CHANGED]);
}

static const char *
font_find_style (const StyleMap *style_map,
		 guint num_styles,
		 const char *style)
{
	guint i;

	g_return_val_if_fail (style_map != NULL, NULL);
	g_return_val_if_fail (num_styles > 0, NULL);
	g_return_val_if_fail (style != NULL, NULL);

	for (i = 0; i < num_styles; i++) {
		if (nautilus_istr_is_equal (style, style_map[i].name)) {
			return _(style_map[i].mapped);
		}
	}

	return NULL;
}

static gboolean
ignore_font (const char *foundry,
	     const char *family) 
{
	guint i;

	g_return_val_if_fail (foundry != NULL, TRUE);
	g_return_val_if_fail (family != NULL, TRUE);

	for (i = 0; i < NAUTILUS_N_ELEMENTS (black_listed_font_families); i++) {
		if (nautilus_istr_is_equal (family, black_listed_font_families[i])) {
			return TRUE;
		}
	}

	for (i = 0; i < NAUTILUS_N_ELEMENTS (black_listed_font_foundries); i++) {
		if (nautilus_istr_is_equal (foundry, black_listed_font_foundries[i])) {
			return TRUE;
		}
	}

	return FALSE;
}

static char *
font_make_name (const char *foundry,
		const char *family)
{
	g_return_val_if_fail (foundry != NULL, NULL);
	g_return_val_if_fail (family != NULL, NULL);
	
	return g_strdup_printf ("%s %s", foundry, family);
}

static char *
font_make_style (const char *weight,
		 const char *slant,
		 const char *set_width,
		 const char *char_set_registry,
		 const char *char_set_encoding)
{
	const char *mapped_weight;
	const char *mapped_slant;
	const char *mapped_set_width;

	g_return_val_if_fail (weight != NULL, NULL);
	g_return_val_if_fail (slant != NULL, NULL);
	g_return_val_if_fail (set_width != NULL, NULL);
	g_return_val_if_fail (char_set_registry != NULL, NULL);
	g_return_val_if_fail (char_set_encoding != NULL, NULL);

	mapped_weight = font_find_style (font_weight_map,
					 NAUTILUS_N_ELEMENTS (font_weight_map),
					 weight);

	mapped_slant = font_find_style (font_slant_map,
					NAUTILUS_N_ELEMENTS (font_slant_map),
					slant);

	mapped_set_width = font_find_style (font_set_width_map,
					    NAUTILUS_N_ELEMENTS (font_set_width_map),
					    set_width);
	
	if (mapped_weight != NULL) {
		/* "abnormal" weight */
		if (mapped_slant == NULL && mapped_set_width == NULL) {
			return g_strdup (mapped_weight);
		} else if (mapped_slant != NULL && mapped_set_width != NULL) {
			return g_strdup_printf ("%s %s %s", mapped_weight, mapped_slant, mapped_set_width);
		} else if (mapped_slant != NULL) {
			return g_strdup_printf ("%s %s", mapped_weight, mapped_slant);
		}

		return g_strdup_printf ("%s %s", mapped_weight, mapped_set_width);
	} else {
		/* normal weight */
		if (mapped_slant == NULL && mapped_set_width == NULL) {
			return g_strdup (_(normal_style));
		} else if (mapped_slant != NULL && mapped_set_width != NULL) {
			return g_strdup_printf ("%s %s", mapped_slant, mapped_set_width);
		} else if (mapped_slant != NULL) {
			return g_strdup_printf ("%s", mapped_slant);
		}
		
		return g_strdup_printf ("%s", mapped_set_width);
	}

	return g_strdup (_(unknown_style));
}

/* Global font list */
static void
global_font_list_free (void)
{
 	GList *font_node;
	FontEntry *font_entry;
 	GList *style_node;
	FontStyleEntry *style_entry;

	if (global_font_list == NULL) {
		return;
	}

	for (font_node = global_font_list; font_node != NULL; font_node = font_node->next) {
		g_assert (font_node->data != NULL);
		font_entry = font_node->data;
		
		g_free (font_entry->name);
		g_free (font_entry->family);
		g_free (font_entry->name_for_display);

		for (style_node = font_entry->style_list; style_node != NULL; style_node = style_node->next) {
			g_assert (style_node->data != NULL);
			style_entry = style_node->data;
			g_free (style_entry->name);
			g_free (style_entry->font_file_name);
			g_free (style_entry);
		}
		g_list_free (font_entry->style_list);

		g_free (font_entry);
	}

	global_font_list = NULL;
}

static guint
font_list_count_families (const GList *font_list,
			  const char *family)
{
	guint count = 0;
	const GList *node;
	const FontEntry *entry;

	g_return_val_if_fail (font_list != NULL, 0);
	g_return_val_if_fail (family != NULL, 0);
	
	for (node = font_list; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		entry = node->data;

		if (nautilus_istr_is_equal (family, entry->family)) {
			count++;
		}
	}

	return count;
}

static const GList *
global_font_list_get (void)
{
	guint family_count = 0;
	GList *node;
	FontEntry *entry;

	if (global_font_list != NULL) {
		return global_font_list;
	}

	nautilus_font_manager_for_each_font (global_font_list_populate_callback, &global_font_list);
	g_assert (global_font_list != NULL);

	for (node = global_font_list; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		entry = node->data;

		g_assert (entry->name_for_display == NULL);
		
		family_count = font_list_count_families (global_font_list, entry->family);
		g_assert (family_count > 0);

		entry->name_for_display = (family_count > 1) ? g_strdup (entry->name) : g_strdup (entry->family);
	}

	g_atexit (global_font_list_free);

	return global_font_list;
}

static gboolean
global_font_list_populate_callback (const char *font_file_name,
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
	GList **font_list;
	char *entry_name;
	FontEntry *entry;
	FontStyleEntry *style;

	g_return_val_if_fail (font_file_name != NULL, FALSE);
	g_return_val_if_fail (foundry != NULL, FALSE);
	g_return_val_if_fail (family != NULL, FALSE);
	g_return_val_if_fail (weight != NULL, FALSE);
	g_return_val_if_fail (slant != NULL, FALSE);
	g_return_val_if_fail (set_width != NULL, FALSE);
	g_return_val_if_fail (char_set_registry != NULL, FALSE);
	g_return_val_if_fail (char_set_encoding != NULL, FALSE);
	g_return_val_if_fail (callback_data != NULL, FALSE);

	font_list = callback_data;

	if (ignore_font (foundry, family)) {
		return TRUE;
	}

	entry_name = font_make_name (foundry, family);
	entry = font_list_find (*font_list, entry_name);

	if (entry == NULL) {
		entry = g_new0 (FontEntry, 1);
		entry->name = g_strdup (entry_name);
		entry->family = g_strdup (family);
		*font_list = g_list_append (*font_list, entry);
	}
	g_assert (entry != NULL);
	g_assert (font_list_find (*font_list, entry_name) == entry);
	g_free (entry_name);

	style = g_new0 (FontStyleEntry, 1);
	style->name = font_make_style (weight, slant, set_width, char_set_registry, char_set_encoding);
	style->font_file_name = g_strdup (font_file_name);

	entry->style_list = g_list_append (entry->style_list, style);

	return TRUE;
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
	const GList *font_list;
	char *result = NULL;
	char *selected_font;
	char *selected_style;
	const FontEntry *entry;
	const GList *node;

	g_return_val_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker), NULL);

	font_list = global_font_list_get ();
	g_assert (font_list != NULL);
	
	selected_font = nautilus_string_picker_get_selected_string (font_picker->details->font_name_picker);
	g_return_val_if_fail (selected_font != NULL, NULL);
	
	selected_style = nautilus_string_picker_get_selected_string (font_picker->details->style_picker);
	g_return_val_if_fail (selected_style != NULL, NULL);
	
	entry = font_list_find (font_list, selected_font);
	g_return_val_if_fail (entry != NULL, NULL);

	node = entry->style_list;
	while (node != NULL && result == NULL) {
		const FontStyleEntry *style;
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
	const GList *font_list;
	const FontEntry *entry;
	const FontStyleEntry *style;
	const GList *font_list_node;
	const GList *style_list_node;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));

	font_list = global_font_list_get ();
	g_assert (font_list != NULL);
	
	font_list_node = font_list;
	while (font_list_node != NULL) {
		g_assert (font_list_node->data != NULL);
		entry = font_list_node->data;

		style_list_node = entry->style_list;
		while (style_list_node != NULL) {
			g_assert (style_list_node->data != NULL);
			style = style_list_node->data;
			
			if (nautilus_istr_is_equal (style->font_file_name, font_name)) {
				nautilus_string_picker_set_selected_string (font_picker->details->font_name_picker,
									    entry->name_for_display);
				font_picker_update_styles (font_picker);
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
