/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-link-set-window.c: window of checkboxes for configuring link sets
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Andy Hertzfeld <andy@eazel.com>
*/

#include <config.h>
#include <stdlib.h>

#include <parser.h>
#include <xmlmemory.h>

#include <libgnomevfs/gnome-vfs.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtktogglebutton.h>

#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-link.h>
#include <libnautilus-extensions/nautilus-link-set.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-xml-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-preferences.h>

#include "nautilus-window.h"
#include "nautilus-link-set-window.h"

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
	gtk_widget_show(checkbox);
	
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
				     (GtkDestroyNotify) g_free);
				     
	gtk_object_set_data_full (GTK_OBJECT (checkbox),
					  "nautilus_link_set_name",
					  g_strdup(name),
					  (GtkDestroyNotify) g_free);
		
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

/* utility routine t o return a list of link set names by iterating the link set directory */

static GList *
get_link_set_names()
{
	GnomeVFSResult result;
	GnomeVFSFileInfo *current_file_info;
	GnomeVFSDirectoryList *list;
	char *link_set_uri, *link_set_name, *dot_pos;
	GList *link_set_list;
	
	link_set_list = NULL;
	
	/* get a uri to the link set directory */
	link_set_uri = g_strdup_printf ("file://%s/share/nautilus/linksets",
					 NAUTILUS_PREFIX);
	
	/* get the directory info */

	result = gnome_vfs_directory_list_load (&list, link_set_uri, GNOME_VFS_FILE_INFO_GETMIMETYPE, NULL, NULL);
	if (result != GNOME_VFS_OK) {
		return NULL;
	}

	/* build the list by iterating through the directory info */	
	for (current_file_info = gnome_vfs_directory_list_first(list); current_file_info != NULL; 
	    current_file_info = gnome_vfs_directory_list_next(list)) {
		link_set_name = g_strdup(current_file_info->name);
			
		/* strip file type suffix */
		dot_pos = strrchr(link_set_name, '.');
		if (dot_pos)
			*dot_pos = '\0';
			
		link_set_list = g_list_prepend(link_set_list, link_set_name);
	}

	gnome_vfs_directory_list_destroy(list);	
	g_free(link_set_uri);

	return link_set_list;	
}

/* create a window used to configure the link sets in the passed in directory */
GtkWindow *
nautilus_link_set_configure_window(const char *directory_path, GtkWindow *window_to_update)
{
	char *title, *temp_str;
	int link_set_count, index;
	GtkWindow *window;
	GtkWidget *checkbox_table, *scrolled_window;
	GList *link_set_names, *current_link_set;

	/* Create the window. */
	window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
  	gtk_container_set_border_width (GTK_CONTAINER (window), GNOME_PAD);
 	/*
 	gtk_widget_set_usize(GTK_WIDGET(window), 240, 160);
 	*/
 		
	/* set the window title */
	title = g_strdup_printf (_("Linksets for %s"), directory_path);
  	gtk_window_set_title (window, title);
	g_free(title);
	
	/* fetch the link set names */
	link_set_names = get_link_set_names();	
	link_set_count = g_list_length(link_set_names);
	
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
	gtk_container_add(GTK_CONTAINER(window), scrolled_window);
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
	
	/* clean up and we're done */
	
	nautilus_g_list_free_deep (link_set_names);
	
	gtk_widget_show(GTK_WIDGET(window));
	return window;	
}


