/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-properties-window.c - window that lets user modify file properties

   Copyright (C) 2000 Eazel, Inc.

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

   Authors: Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "fm-properties-window.h"

#include <string.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkhseparator.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtksignal.h>

#include <libnautilus/nautilus-glib-extensions.h>

/* static GHashTable *windows; */

static const char * const property_names[] =
{
	"certified",
	"changed",
	"confidential",
	"encrypted",
	"important",
	"new",
	"personal",
	"remote"
};

static void
property_button_update (GtkToggleButton *button)
{
	NautilusFile *file;
	char *name;
	GList *keywords, *word;

	file = gtk_object_get_data (GTK_OBJECT (button), "nautilus_file");
	name = gtk_object_get_data (GTK_OBJECT (button), "nautilus_property_name");

	/* Handle the case where there's nothing to toggle. */
	if (file == NULL || nautilus_file_is_gone (file) || name == NULL) {
		gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
		gtk_toggle_button_set_active (button, FALSE);
		return;
	}

	/* Check and see if it's in there. */
	keywords = nautilus_file_get_keywords (file);
	word = g_list_find_custom (keywords, name, (GCompareFunc) strcmp);
	gtk_toggle_button_set_active (button, word != NULL);
}

static void
property_button_toggled (GtkToggleButton *button)
{
	NautilusFile *file;
	char *name;
	GList *keywords, *word;

	file = gtk_object_get_data (GTK_OBJECT (button), "nautilus_file");
	name = gtk_object_get_data (GTK_OBJECT (button), "nautilus_property_name");

	/* Handle the case where there's nothing to toggle. */
	if (file == NULL || nautilus_file_is_gone (file) || name == NULL) {
		return;
	}

	/* Check and see if it's already there. */
	keywords = nautilus_file_get_keywords (file);
	word = g_list_find_custom (keywords, name, (GCompareFunc) strcmp);
	if (gtk_toggle_button_get_active (button)) {
		if (word == NULL) {
			keywords = g_list_append (keywords, g_strdup (name));
		}
	} else {
		if (word != NULL) {
			keywords = g_list_remove_link (keywords, word);
		}
	}
	nautilus_file_set_keywords (file, keywords);
}

static GtkWindow *
create_properties_window (NautilusFile *file)
{
	GtkWindow *window;
	GtkWidget *top_level_vbox, *prompt, *separator_line, *button;
	GtkWidget *check_buttons_box, *left_buttons_box, *right_buttons_box;
	char *name, *title;
	int i;

	/* Compute the title. */
	name = nautilus_file_get_name (file);
	title = g_strdup_printf (_("Nautilus: %s Properties"), name);
	g_free (name);

	/* Create the window. */
	window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
  	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
  	gtk_window_set_title (window, title);
  	gtk_window_set_policy (window, FALSE, FALSE, FALSE);
	g_free (title);

	/* Create the top-level box. */
	top_level_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (top_level_vbox);
	gtk_container_add (GTK_CONTAINER (window), top_level_vbox);

	/* The prompt. */
	prompt = gtk_label_new (_("This is a placeholder for the real properties dialog. For now, you can turn some hard-wired properties with corresponding emblems on and off."));
	gtk_widget_show (prompt);
	gtk_box_pack_start (GTK_BOX (top_level_vbox), prompt, FALSE, FALSE, 0);
   	gtk_label_set_justify (GTK_LABEL (prompt), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (prompt), TRUE);

	/* Separator between prompt and check buttons. */
  	separator_line = gtk_hseparator_new ();
  	gtk_widget_show (separator_line);
  	gtk_box_pack_start (GTK_BOX (top_level_vbox), separator_line, TRUE, TRUE, 8);

	/* Holder for two columns of check buttons. */
	check_buttons_box = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (check_buttons_box);
	gtk_box_pack_start (GTK_BOX (top_level_vbox), check_buttons_box, FALSE, FALSE, 0);

	/* Left column of check buttons. */
	left_buttons_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (left_buttons_box);
	gtk_box_pack_start (GTK_BOX (check_buttons_box), left_buttons_box, FALSE, FALSE, 0);

	/* Right column of check buttons. */
	right_buttons_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (right_buttons_box);
	gtk_box_pack_start (GTK_BOX (check_buttons_box), right_buttons_box, FALSE, FALSE, 0);

	/* The check buttons themselves. */
	for (i = 0; i < NAUTILUS_N_ELEMENTS (property_names); i++) {
		button = gtk_check_button_new_with_label (_(property_names[i]));
		gtk_widget_show (button);

		/* Attach parameters and signal handler. */
		gtk_object_set_data (GTK_OBJECT (button),
				     "nautilus_property_name",
				     (char *) property_names[i]);
		
		gtk_object_set_data (GTK_OBJECT (button),
					  "nautilus_file",
					  file);
		
		gtk_signal_connect (GTK_OBJECT (button),
				    "toggled",
				    property_button_toggled,
				    NULL);

		property_button_update (GTK_TOGGLE_BUTTON (button));

		if (i < NAUTILUS_N_ELEMENTS (property_names) / 2) {
			gtk_box_pack_start (GTK_BOX (left_buttons_box), button, FALSE, FALSE, 0);
		} else {
			gtk_box_pack_start (GTK_BOX (right_buttons_box), button, FALSE, FALSE, 0);
		}
	}

	return window;
}

GtkWindow *
fm_properties_window_get_or_create (NautilusFile *file)
{
	return create_properties_window (file);
}
