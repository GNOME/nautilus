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

#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtksignal.h>

#include <libgnome/gnome-i18n.h>

static const gint FONT_PICKER_SPACING = 10;

/* Signals */
typedef enum
{
	SELECTED_FONT_CHANGED,
	LAST_SIGNAL
} FontPickerSignals;

struct _NautilusFontPickerDetail
{
	GtkWidget		*family_picker;
	GtkWidget		*weight_picker;
	GtkWidget		*slant_picker;
	GtkWidget		*set_width_picker;

	NautilusStringList	*weight_list;
	NautilusStringList	*slant_list;
	NautilusStringList	*set_width_list;
};

/* NautilusFontPickerClass methods */
static void nautilus_font_picker_initialize_class (NautilusFontPickerClass *klass);
static void nautilus_font_picker_initialize       (NautilusFontPicker      *font_picker);


/* GtkObjectClass methods */
static void nautilus_font_picker_destroy          (GtkObject               *object);
static void family_picker_changed_callback        (GtkWidget               *string_picker,
						   gpointer                 user_data);
static void weight_picker_changed_callback        (GtkWidget               *string_picker,
						   gpointer                 user_data);
static void slant_picker_changed_callback         (GtkWidget               *string_picker,
						   gpointer                 user_data);
static void set_width_picker_changed_callback     (GtkWidget               *string_picker,
						   gpointer                 user_data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusFontPicker, nautilus_font_picker, GTK_TYPE_HBOX)

static guint font_picker_signals[LAST_SIGNAL] = { 0 };

/*
 * NautilusFontPickerClass methods
 */
static void
nautilus_font_picker_initialize_class (NautilusFontPickerClass *font_picker_class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (font_picker_class);
	widget_class = GTK_WIDGET_CLASS (font_picker_class);

	/* GtkObjectClass */
	object_class->destroy = nautilus_font_picker_destroy;

	/* Signals */
	font_picker_signals[SELECTED_FONT_CHANGED] = gtk_signal_new ("selected_font_changed",
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
	NautilusStringList *family_list;

	font_picker->detail = g_new (NautilusFontPickerDetail, 1);

	font_picker->detail->weight_list = NULL;
	font_picker->detail->slant_list = NULL;
	font_picker->detail->set_width_list = NULL;

	gtk_box_set_homogeneous (GTK_BOX (font_picker), FALSE);
	gtk_box_set_spacing (GTK_BOX (font_picker), FONT_PICKER_SPACING);

	font_picker->detail->family_picker = nautilus_string_picker_new ();
	font_picker->detail->weight_picker = nautilus_string_picker_new ();
	font_picker->detail->slant_picker = nautilus_string_picker_new ();
	font_picker->detail->set_width_picker = nautilus_string_picker_new ();

	gtk_box_pack_start (GTK_BOX (font_picker), font_picker->detail->family_picker, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (font_picker), font_picker->detail->weight_picker, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (font_picker), font_picker->detail->slant_picker, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (font_picker), font_picker->detail->set_width_picker, TRUE, TRUE, 0);

	nautilus_caption_set_title_label (NAUTILUS_CAPTION (font_picker->detail->family_picker), _("Font"));

 	nautilus_caption_set_show_title (NAUTILUS_CAPTION (font_picker->detail->family_picker), FALSE);
 	nautilus_caption_set_show_title (NAUTILUS_CAPTION (font_picker->detail->weight_picker), FALSE);
 	nautilus_caption_set_show_title (NAUTILUS_CAPTION (font_picker->detail->slant_picker), FALSE);
 	nautilus_caption_set_show_title (NAUTILUS_CAPTION (font_picker->detail->set_width_picker), FALSE);

	family_list = nautilus_scalable_font_get_font_family_list ();

	nautilus_string_list_sort (family_list);
	
	/* FIXME bugzilla.eazel.com 2557: 
	 * Need to deal with possiblity of there being no fonts */
	g_assert (family_list != NULL);

	nautilus_string_picker_set_string_list (NAUTILUS_STRING_PICKER (font_picker->detail->family_picker),
						family_list);
	
	nautilus_string_list_free (family_list);

	gtk_signal_connect (GTK_OBJECT (font_picker->detail->family_picker),
			    "changed",
			    GTK_SIGNAL_FUNC (family_picker_changed_callback),
			    font_picker);

	gtk_signal_connect (GTK_OBJECT (font_picker->detail->weight_picker),
			    "changed",
			    GTK_SIGNAL_FUNC (weight_picker_changed_callback),
			    font_picker);

	gtk_signal_connect (GTK_OBJECT (font_picker->detail->slant_picker),
			    "changed",
			    GTK_SIGNAL_FUNC (slant_picker_changed_callback),
			    font_picker);

	gtk_signal_connect (GTK_OBJECT (font_picker->detail->set_width_picker),
			    "changed",
			    GTK_SIGNAL_FUNC (set_width_picker_changed_callback),
			    font_picker);

	gtk_widget_show (font_picker->detail->family_picker);
	gtk_widget_show (font_picker->detail->weight_picker);
	gtk_widget_show (font_picker->detail->slant_picker);
	gtk_widget_show (font_picker->detail->set_width_picker);

	family_picker_changed_callback (font_picker->detail->family_picker, font_picker);
}

/* GtkObjectClass methods */
static void
nautilus_font_picker_destroy (GtkObject* object)
{
	NautilusFontPicker * font_picker;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (object));
	
	font_picker = NAUTILUS_FONT_PICKER (object);
	
	nautilus_string_list_free (font_picker->detail->weight_list);
	nautilus_string_list_free (font_picker->detail->slant_list);
	nautilus_string_list_free (font_picker->detail->set_width_list);

	g_free (font_picker->detail);

	/* Chain */
	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
font_picker_update_weight_picker (NautilusFontPicker *font_picker)
{
	NautilusStringList	*unique_weight_list;
	char			*family;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));
	
	family = nautilus_string_picker_get_selected_string (NAUTILUS_STRING_PICKER (font_picker->detail->family_picker));

	unique_weight_list = nautilus_string_list_new_from_string_list (font_picker->detail->weight_list, TRUE);
	
	nautilus_string_list_sort (unique_weight_list);
	nautilus_string_list_remove_duplicates (unique_weight_list);
	
	nautilus_string_picker_set_string_list (NAUTILUS_STRING_PICKER (font_picker->detail->weight_picker),
						unique_weight_list);
	
	nautilus_string_list_free (unique_weight_list);

	g_free (family);
}

static void
font_picker_update_slant_picker (NautilusFontPicker *font_picker)
{
 	NautilusStringList	*unique_slant_list;
	char			*current_family;
	char			*current_weight;
	guint			i;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));
	
	current_family = nautilus_string_picker_get_selected_string (NAUTILUS_STRING_PICKER (font_picker->detail->family_picker));
	current_weight = nautilus_string_picker_get_selected_string (NAUTILUS_STRING_PICKER (font_picker->detail->weight_picker));

	g_assert (nautilus_string_list_get_length (font_picker->detail->slant_list) ==
		  nautilus_string_list_get_length (font_picker->detail->weight_list));

	unique_slant_list = nautilus_string_list_new (TRUE);

	for (i = 0; i < nautilus_string_list_get_length (font_picker->detail->slant_list); i++) {
		char *weight = nautilus_string_list_nth (font_picker->detail->weight_list, i);
		char *slant = nautilus_string_list_nth (font_picker->detail->slant_list, i);

		if (nautilus_str_is_equal (current_weight, weight)) {
			nautilus_string_list_insert (unique_slant_list, slant);
		}

		g_free (weight);
		g_free (slant);
	}

	nautilus_string_list_sort (unique_slant_list);
	nautilus_string_list_remove_duplicates (unique_slant_list);

	nautilus_string_picker_set_string_list (NAUTILUS_STRING_PICKER (font_picker->detail->slant_picker),
						unique_slant_list);
						
 	nautilus_string_list_free (unique_slant_list);

	g_free (current_family);
	g_free (current_weight);
}

static void
font_picker_update_set_width_picker (NautilusFontPicker *font_picker)
{
 	NautilusStringList	*unique_set_width_list;
	char			*current_family;
	char			*current_weight;
	char			*current_slant;
	guint			i;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));
	
	current_family = nautilus_string_picker_get_selected_string (NAUTILUS_STRING_PICKER (font_picker->detail->family_picker));
	current_weight = nautilus_string_picker_get_selected_string (NAUTILUS_STRING_PICKER (font_picker->detail->weight_picker));
	current_slant = nautilus_string_picker_get_selected_string (NAUTILUS_STRING_PICKER (font_picker->detail->slant_picker));
	
	g_assert (nautilus_string_list_get_length (font_picker->detail->slant_list) ==
		  nautilus_string_list_get_length (font_picker->detail->weight_list));

	g_assert (nautilus_string_list_get_length (font_picker->detail->slant_list) ==
		  nautilus_string_list_get_length (font_picker->detail->set_width_list));

	unique_set_width_list = nautilus_string_list_new (TRUE);

	for (i = 0; i < nautilus_string_list_get_length (font_picker->detail->set_width_list); i++) {
		char *weight = nautilus_string_list_nth (font_picker->detail->weight_list, i);
		char *slant = nautilus_string_list_nth (font_picker->detail->slant_list, i);
		char *set_width = nautilus_string_list_nth (font_picker->detail->set_width_list, i);

		if (nautilus_str_is_equal (current_weight, weight)
		    && nautilus_str_is_equal (current_slant, slant)) {
			nautilus_string_list_insert (unique_set_width_list, set_width);
		}

		g_free (weight);
		g_free (slant);
		g_free (set_width);
	}
	
	nautilus_string_list_sort (unique_set_width_list);
	nautilus_string_list_remove_duplicates (unique_set_width_list);

	nautilus_string_picker_set_string_list (NAUTILUS_STRING_PICKER (font_picker->detail->set_width_picker),
						unique_set_width_list);
						
 	nautilus_string_list_free (unique_set_width_list);

	g_free (current_family);
	g_free (current_weight);
	g_free (current_slant);
}

static void
family_picker_changed_callback (GtkWidget *string_picker, gpointer user_data)
{
	NautilusFontPicker	*font_picker;
	char			*family;
	NautilusStringList	*weight_list = NULL;
	NautilusStringList	*slant_list = NULL;
	NautilusStringList	*set_width_list = NULL;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (user_data));
	g_return_if_fail (NAUTILUS_IS_STRING_PICKER (string_picker));

	font_picker = NAUTILUS_FONT_PICKER (user_data);

	family = nautilus_string_picker_get_selected_string (NAUTILUS_STRING_PICKER (string_picker));
	
	if (nautilus_scalable_font_query_font (family, &weight_list, &slant_list, &set_width_list)) {
		nautilus_string_list_free (font_picker->detail->weight_list);
		nautilus_string_list_free (font_picker->detail->slant_list);
		nautilus_string_list_free (font_picker->detail->set_width_list);

		font_picker->detail->weight_list = weight_list;
		font_picker->detail->slant_list = slant_list;
		font_picker->detail->set_width_list = set_width_list;
		
		font_picker_update_weight_picker (font_picker);
		font_picker_update_slant_picker (font_picker);
		font_picker_update_set_width_picker (font_picker);

		gtk_signal_emit (GTK_OBJECT (font_picker), font_picker_signals[SELECTED_FONT_CHANGED]);
	}
	else {
		g_warning ("Trying to set a bogus non existant font '%s'\n", family);
	}

	g_free (family);
}

static void
weight_picker_changed_callback (GtkWidget *string_picker, gpointer user_data)
{
	NautilusFontPicker	*font_picker;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (user_data));
	g_return_if_fail (NAUTILUS_IS_STRING_PICKER (string_picker));

	font_picker = NAUTILUS_FONT_PICKER (user_data);

	gtk_signal_emit (GTK_OBJECT (font_picker), font_picker_signals[SELECTED_FONT_CHANGED]);
}

static void
slant_picker_changed_callback (GtkWidget *string_picker, gpointer user_data)
{
	NautilusFontPicker	*font_picker;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (user_data));
	g_return_if_fail (NAUTILUS_IS_STRING_PICKER (string_picker));

	font_picker = NAUTILUS_FONT_PICKER (user_data);

	gtk_signal_emit (GTK_OBJECT (font_picker), font_picker_signals[SELECTED_FONT_CHANGED]);
}

static void
set_width_picker_changed_callback (GtkWidget *string_picker, gpointer user_data)
{
	NautilusFontPicker	*font_picker;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (user_data));
	g_return_if_fail (NAUTILUS_IS_STRING_PICKER (string_picker));

	font_picker = NAUTILUS_FONT_PICKER (user_data);

	gtk_signal_emit (GTK_OBJECT (font_picker), font_picker_signals[SELECTED_FONT_CHANGED]);
}


/*
 * NautilusFontPicker public methods
 */
GtkWidget *
nautilus_font_picker_new (void)
{
	return gtk_widget_new (nautilus_font_picker_get_type (), NULL);
}

void
nautilus_font_picker_set_selected_family (NautilusFontPicker	*font_picker,
					  const char		*family)
{
	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));
	g_return_if_fail (family != NULL);

	if (!nautilus_string_picker_contains (NAUTILUS_STRING_PICKER (font_picker->detail->family_picker), family)) {
		g_warning ("Trying to set a bogus family '%s'\n", family);
		return;
	}
	
	nautilus_string_picker_set_selected_string (NAUTILUS_STRING_PICKER (font_picker->detail->family_picker),
						    family);
}

void
nautilus_font_picker_set_selected_weight (NautilusFontPicker	*font_picker,
					  const char		*weight)
{
	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));
	g_return_if_fail (weight != NULL);

	if (!nautilus_string_picker_contains (NAUTILUS_STRING_PICKER (font_picker->detail->weight_picker), weight)) {
		g_warning ("Trying to set a bogus weight '%s'\n", weight);
		return;
	}
	
	nautilus_string_picker_set_selected_string (NAUTILUS_STRING_PICKER (font_picker->detail->weight_picker),
						    weight);
}

void
nautilus_font_picker_set_selected_slant (NautilusFontPicker	*font_picker,
					  const char		*slant)
{
	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));
	g_return_if_fail (slant != NULL);

	if (!nautilus_string_picker_contains (NAUTILUS_STRING_PICKER (font_picker->detail->slant_picker), slant)) {
		g_warning ("Trying to set a bogus slant '%s'\n", slant);
		return;
	}
	
	nautilus_string_picker_set_selected_string (NAUTILUS_STRING_PICKER (font_picker->detail->slant_picker),
						    slant);
}

void
nautilus_font_picker_set_selected_set_width (NautilusFontPicker	*font_picker,
					  const char		*set_width)
{
	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));
	g_return_if_fail (set_width != NULL);

	if (!nautilus_string_picker_contains (NAUTILUS_STRING_PICKER (font_picker->detail->set_width_picker), set_width)) {
		g_warning ("Trying to set a bogus set_width '%s'\n", set_width);
		return;
	}
	
	nautilus_string_picker_set_selected_string (NAUTILUS_STRING_PICKER (font_picker->detail->set_width_picker),
						    set_width);
}

char *
nautilus_font_picker_get_selected_family (const NautilusFontPicker *font_picker)
{
	g_return_val_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker), NULL);

	return nautilus_string_picker_get_selected_string (NAUTILUS_STRING_PICKER (font_picker->detail->family_picker));
}

char *
nautilus_font_picker_get_selected_weight (const NautilusFontPicker *font_picker)
{
	g_return_val_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker), NULL);

	return nautilus_string_picker_get_selected_string (NAUTILUS_STRING_PICKER (font_picker->detail->weight_picker));
}

char *
nautilus_font_picker_get_selected_slant (const NautilusFontPicker *font_picker)
{
	g_return_val_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker), NULL);

	return nautilus_string_picker_get_selected_string (NAUTILUS_STRING_PICKER (font_picker->detail->slant_picker));
}

char *
nautilus_font_picker_get_selected_set_width (const NautilusFontPicker *font_picker)
{
	g_return_val_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker), NULL);

	return nautilus_string_picker_get_selected_string (NAUTILUS_STRING_PICKER (font_picker->detail->set_width_picker));
}
