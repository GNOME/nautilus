/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 */

/* nautilus-link-set-window.c: window of checkboxes for configuring link sets
 */

#include <config.h>
#include "nautilus-link-set-window.h"

#include "nautilus-window.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-string.h>
#include <eel/eel-xml-extensions.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktable.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkwindow.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-link-set.h>
#include <libnautilus-private/nautilus-link.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <stdlib.h>

/* global to hold the currently allocated link set window, if any */

static GtkWindow *link_set_window = NULL;

/* handle the check box toggling */
static void
link_set_check_box_toggled (GtkToggleButton *button, GtkWindow *window_to_update)
{
	char *path, *name;
	
	path = gtk_object_get_data (GTK_OBJECT (button), "nautilus_directory_path");
	name = gtk_object_get_data (GTK_OBJECT (button), "nautilus_link_set_name");
	
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
		nautilus_link_set_install(path, name);
	else
		nautilus_link_set_remove(path, name);	

	/* tell the associated window to reload in order to display the new links (or lack thereof) */	
	nautilus_window_reload (NAUTILUS_WINDOW(window_to_update));
}

/* utility to make a link set checkbox that installs and removes it's corresponding
   link set when toggled */

static void
make_link_set_check_box(const char *directory_path, GtkWidget *checkbox_table, 
			int index, char *name, GtkWindow *window)
{
	GtkWidget *checkbox, *label;
	
	/* add a checkbox and a label */
		
	checkbox = gtk_check_button_new ();				
	gtk_widget_show (checkbox);
	
	label = gtk_label_new (name);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show(label);
		
	gtk_container_add (GTK_CONTAINER (checkbox), label);
	gtk_widget_show(checkbox);

	/* Set initial state of checkbox. */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), nautilus_link_set_is_installed(directory_path, name));
			
	/* Attach the parameters and a signal handler. */
	gtk_object_set_data_full (GTK_OBJECT (checkbox),
				  "nautilus_directory_path",
				  g_strdup (directory_path),
				  g_free);
				     
	gtk_object_set_data_full (GTK_OBJECT (checkbox),
				  "nautilus_link_set_name",
				  g_strdup(name),
				  g_free);
		
	gtk_signal_connect (GTK_OBJECT (checkbox),
			    "toggled",
			    link_set_check_box_toggled,
			    window);


	/* attach the checkbox to the table */
	if (index % 2) {
			gtk_table_attach_defaults (GTK_TABLE (checkbox_table), checkbox,
					  	   0, 1,
					  	   index >> 1, (index >> 1) + 1);
		} else {
			gtk_table_attach_defaults (GTK_TABLE (checkbox_table), checkbox,
						   1, 2,
					  	   index >> 1, (index >> 1) + 1);
		}
}

/* utility routine to return a list of link set names by iterating the link set directory */
static GList *
get_link_set_names (void)
{
	GnomeVFSResult result;
	GnomeVFSFileInfo *current_file_info;
	GList *list, *node;
	char *link_set_uri, *link_set_name, *dot_pos;
	GList *link_set_list;
	
	/* get a uri to the link set directory */
	link_set_uri = gnome_vfs_get_uri_from_local_path (NAUTILUS_DATADIR "/linksets");
	
	/* get the directory info */
	result = gnome_vfs_directory_list_load (&list, link_set_uri, 
						GNOME_VFS_FILE_INFO_FOLLOW_LINKS,
						NULL);
	if (result != GNOME_VFS_OK) {
		g_free (link_set_uri);
		return NULL;
	}

	link_set_list = NULL;
	
	/* FIXME bugzilla.gnome.org 45049: The names should really come from the names inside the files. */
	/* build the list by iterating through the directory info */	
	for (node = list; node != NULL; node = node->next) {
		current_file_info = node->data;
		link_set_name = g_strdup (current_file_info->name);
			
		/* strip file type suffix */
		dot_pos = strrchr (link_set_name, '.');
		if (dot_pos != NULL) {
			*dot_pos = '\0';
		}

		link_set_list = g_list_prepend (link_set_list, link_set_name);
	}

	gnome_vfs_file_info_list_free (list);	
	g_free (link_set_uri);

	return eel_g_str_list_alphabetize (link_set_list);
}

/* create a window used to configure the link sets in the passed in directory */
GtkWindow *
nautilus_link_set_configure_window (const char *directory_path, GtkWindow *window_to_update)
{		
	char *temp_str;
	int link_set_count, index;
	GtkWindow *window;
	GtkWidget *vbox, *label;
	GtkWidget *checkbox_table, *scrolled_window;
	GList *link_set_names, *current_link_set;

	/* Create the window. */
	window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
  	gtk_container_set_border_width (GTK_CONTAINER (window), GNOME_PAD);
 	gtk_widget_set_usize(GTK_WIDGET(window), 280, 120);
 		
	/* set the window title */
  	gtk_window_set_title (window, _("Link sets"));
	gtk_window_set_wmclass (window, "link_sets", "Nautilus");
	
	/* fetch the link set names */
	link_set_names = get_link_set_names ();	
	link_set_count = g_list_length (link_set_names);
	
	/* allocate a vbox to hold the contents of the window */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_widget_show(vbox);
	
	/* add a descriptive label */
	label = gtk_label_new(_("Add or remove sets of links by clicking on the checkboxes below."));
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 4);
	gtk_widget_show(label);
	
	/* allocate a table to hold the link set checkboxes */
	checkbox_table = gtk_table_new ((link_set_count + 1) / 2, 2, TRUE);
	gtk_widget_show (checkbox_table);
	gtk_container_set_border_width (GTK_CONTAINER (checkbox_table), GNOME_PAD);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), 
					       checkbox_table);
	gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
	gtk_widget_show (scrolled_window);
	
	/* iterate the directory to add a checkbox for each linkset found */
	index = 0;
	for (current_link_set = link_set_names; current_link_set != NULL; 
		current_link_set = current_link_set->next) {
			temp_str = (char *) current_link_set->data;
			if ((temp_str[0] != '.') && (temp_str[0] != '\0'))
				make_link_set_check_box(directory_path, checkbox_table, 
						index++, temp_str, window_to_update);	
	}
		
	eel_g_list_free_deep (link_set_names);

	/* show the window */
	gtk_window_set_position(window, GTK_WIN_POS_CENTER);
	gtk_widget_show(GTK_WIDGET(window));
	return window;	
}

/* callback to clear the window global when the window is deleted */
static gboolean
delete_window_callback (GtkWidget *widget,
			GdkEvent *event,
			gpointer user_data)
{
	link_set_window = NULL;
	return FALSE;
}

/* toggle the visiblity of the link set window */
GtkWindow *
nautilus_link_set_toggle_configure_window (const char *directory_path, GtkWindow *window_to_update)
{
	if (link_set_window != NULL) {
		gtk_widget_destroy (GTK_WIDGET (link_set_window));
		link_set_window = NULL;
	} else {
		link_set_window = nautilus_link_set_configure_window (directory_path, 
								      window_to_update);
		
		gtk_signal_connect (GTK_OBJECT (link_set_window), "delete_event",
				    GTK_SIGNAL_FUNC (delete_window_callback), NULL);
	}
									
	return link_set_window;		
}
