/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* test-mime.c - Test for the mime handler detection features of the GNOME
   Virtual File System Library

   Copyright (C) 2000 Eazel

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

   Author: Maciej Stachowiak <mjs@eazel.com>
*/

#include <config.h>

#include <gnome.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-private/nautilus-mime-actions.h>
#include <stdio.h>

static gboolean ready = FALSE;


static void
append_comma_and_scheme (gpointer scheme,
			 gpointer user_data)
{
	char **string;

	string = (char **) user_data;
	if (strlen (*string) > 0) {
		*string = g_strconcat (*string, ", ", scheme, NULL);
	}
	else {
		*string = g_strdup (scheme);
	}
}


static char *
format_supported_uri_schemes_for_display (GList *supported_uri_schemes)
{
	char *string;

	string = g_strdup ("");
	g_list_foreach (supported_uri_schemes,
			append_comma_and_scheme,
			&string);
	return string;
}

static void
print_application (GnomeVFSMimeApplication *application)
{
        if (application == NULL) {
	        puts ("(none)");
	} else {
	        printf ("name: %s\ncommand: %s\ncan_open_multiple_files: %s\nexpects_uris: %s\nsupported_uri_schemes: %s\nrequires_terminal: %s\n", 
			application->name, application->command, 
			(application->can_open_multiple_files ? "TRUE" : "FALSE"),
			(application->expects_uris ? "TRUE" : "FALSE"),
			format_supported_uri_schemes_for_display (application->supported_uri_schemes),
			(application->requires_terminal ? "TRUE" : "FALSE"));
	}
}

static void
print_component (OAF_ServerInfo *component)
{
        if (component == NULL) {
	        puts ("(none)");
	} else {
	        printf ("iid: %s\n", component->iid);
	}
}

static void
print_action (GnomeVFSMimeAction *action)
{
        if (action == NULL) {
	        puts ("(none)");
	} else {
		if (action->action_type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION) {
			puts ("type: application");
			print_application (action->action.application);
		} else {
			puts ("type: component");
			print_component (action->action.component);
		}
	}
}


static void 
print_component_list (GList *components)
{
	GList *p;
	
	if (components == NULL) {
		puts ("(none)");
	} else {
		for (p = components; p != NULL; p = p->next) {
			print_component (p->data);
			puts ("------");
		}
	}
}

static void 
print_application_list (GList *applications)
{
	GList *p;

	if (applications == NULL) {
		puts ("(none)");
	} else {
		for (p = applications; p != NULL; p = p->next) {
			print_application (p->data);
			puts ("------");
		}
	}
}

static void
ready_callback (NautilusFile *file,
		gpointer callback_data)
{
	ready = TRUE;
}

int
main (int argc, char **argv)
{
        const char *uri;  
	GnomeVFSMimeApplication *default_application;
	OAF_ServerInfo *default_component;
	GnomeVFSMimeAction *default_action;
	GList *all_components;
	GList *all_applications;
	GList *short_list_components;
	GList *short_list_applications;
	NautilusFile *file;
	GList *attributes;

	g_thread_init (NULL);
	gnome_vfs_init ();

	gnomelib_register_popt_table (oaf_popt_options, oaf_get_popt_table_name ());
	oaf_init (argc, argv);
	gnome_init("test-nautilus-mime-actions", "0.0",
		       argc, argv);

	if (argc != 2) {
		fprintf (stderr, "Usage: %s uri\n", *argv);
		return 1;
	}

	uri = argv[1];
	file = nautilus_file_get (uri);

	attributes = nautilus_mime_actions_get_full_file_attributes ();
	nautilus_file_call_when_ready (file, attributes, ready_callback, NULL);
	g_list_free (attributes);

	while (!ready) {
		gtk_main_iteration ();
	}

	default_action = nautilus_mime_get_default_action_for_file (file);
	puts ("Default Action");
	print_action (default_action);
	puts ("");

	default_application = nautilus_mime_get_default_application_for_file (file);
	puts("Default Application");
	print_application (default_application);
	puts ("");
		
	default_component = nautilus_mime_get_default_component_for_file (file);
	puts("Default Component");
	print_component (default_component);
	puts ("");

	short_list_applications = nautilus_mime_get_short_list_applications_for_file (file); 
	puts("Short List Applications");
	print_application_list (short_list_applications);
	puts ("");

	short_list_components = nautilus_mime_get_short_list_components_for_file (file); 
	puts("Short List Components");
	print_component_list (short_list_components);
	puts ("");

	all_applications = nautilus_mime_get_all_applications_for_file (file); 
	puts("All Applications");
	print_application_list (all_applications);
	puts ("");

	all_components = nautilus_mime_get_all_components_for_file (file); 
	puts("All Components");
	print_component_list (all_components);
	puts ("");

	return 0;
}


