/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Andy Hertzfeld <andy@eazel.com>
 *
 */

/* annotation metaview - allows you to annotate a directory or file */

#include <config.h>

#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>
#include <ctype.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-debug.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-file-attributes.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-font-factory.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>

/* FIXME bugzilla.eazel.com 4436: 
 * Undo not working in notes-view.
 */
#if 0
#include <libnautilus-extensions/nautilus-undo-signal-handlers.h>
#endif
#include <libnautilus/libnautilus.h>
#include <libnautilus/nautilus-clipboard.h>
#include <liboaf/liboaf.h>
#include <limits.h>

#define NOTES_DEFAULT_BACKGROUND_COLOR "rgb:FFFF/FFFF/BBBB"


typedef struct {
        NautilusView *view;
        GtkWidget *note_text_field;
        char *uri;
        NautilusFile *file;
} Notes;

static int notes_object_count = 0;

static void
set_note_text_from_metadata (NautilusFile *file,
                     Notes *notes)
{
        int position;
        char *saved_text;
        char *current_text;

        g_assert (NAUTILUS_IS_FILE (file));
        g_assert (notes->file == file);

        saved_text = nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_ANNOTATION, "");
	current_text = gtk_editable_get_chars (GTK_EDITABLE (notes->note_text_field), 0, -1);

	/* This fn is called for any change signal on the file, so make sure that the
	 * metadata has actually changed.
	 */
        if (strcmp (saved_text, current_text) != 0) {
	        gtk_editable_delete_text (GTK_EDITABLE (notes->note_text_field), 0, -1);
	        
	        position = 0;
	        gtk_editable_insert_text (GTK_EDITABLE (notes->note_text_field),
	                                  saved_text,
	                                  strlen (saved_text),
	                                  &position);
	}
	
	g_free (saved_text);
	g_free (current_text);

/* FIXME bugzilla.eazel.com 4436: 
 * Undo not working in notes-view.
 */
#if 0
	nautilus_undo_set_up_editable_for_undo (GTK_EDITABLE (notes->note_text_field));
#endif
}

static void
done_with_file (Notes *notes)
{
	if (notes->file != NULL) {
		nautilus_file_monitor_remove (notes->file, notes);
		gtk_signal_disconnect_by_func (GTK_OBJECT (notes->file),
					       GTK_SIGNAL_FUNC (set_note_text_from_metadata),
					       notes);
	        nautilus_file_unref (notes->file);
        }
}

static void
notes_load_metainfo (Notes *notes)
{
        GList *attributes;

        gtk_editable_delete_text (GTK_EDITABLE (notes->note_text_field), 0, -1);   
        
        done_with_file (notes);
        notes->file = nautilus_file_get (notes->uri);

        if (notes->file == NULL) {
                return;
        }

        attributes = g_list_append (NULL, NAUTILUS_FILE_ATTRIBUTE_METADATA);
        nautilus_file_monitor_add (notes->file, notes, attributes);

	if (nautilus_file_check_if_ready (notes->file, attributes)) {
		set_note_text_from_metadata (notes->file, notes);
	}
	
        g_list_free (attributes);
        
	gtk_signal_connect (GTK_OBJECT (notes->file),
			    "changed",
			    GTK_SIGNAL_FUNC (set_note_text_from_metadata),
			    notes);
}

/* save the metainfo corresponding to the current uri, if any, into the text field */

static void
notes_save_metainfo (Notes *notes)
{
        char *notes_text;

        if (notes->file == NULL) {
                return;
        }

        /* Block the handler, so we don't respond to our own change.
         */
        gtk_signal_handler_block_by_func (GTK_OBJECT (notes->file),
                                          set_note_text_from_metadata,
                                          notes);
                                          
        notes_text = gtk_editable_get_chars (GTK_EDITABLE (notes->note_text_field), 0 , -1);
        nautilus_file_set_metadata (notes->file, NAUTILUS_METADATA_KEY_ANNOTATION, NULL, notes_text);
        g_free (notes_text);
        
        gtk_signal_handler_unblock_by_func (GTK_OBJECT (notes->file),
                                            set_note_text_from_metadata,
                                            notes);
}

static void
notes_load_location (NautilusView *view,
                     const char *location,
                     Notes *notes)
{
        if (strcmp (notes->uri, location) != 0) {
/* FIXME bugzilla.eazel.com 4436: 
 * Undo not working in notes-view.
 */
#if 0
		nautilus_undo_tear_down_editable_for_undo (GTK_EDITABLE (notes->note_text_field));
#endif
                notes_save_metainfo (notes);
                g_free (notes->uri);
                notes->uri = g_strdup (location);
                notes_load_metainfo (notes);
        }
}

static gboolean
on_text_field_focus_out_event (GtkWidget *widget,
			       GdkEventFocus *event,
			       gpointer user_data)
{
	Notes *notes = user_data;

	notes_save_metainfo (notes);
	return FALSE;
}

                              
static void
do_destroy (GtkObject *obj, Notes *notes)
{
	/* If the widget is being destroyed first, make sure the bonobo object
	 * that owns it is not destroyed half-way through the widget destroy
	 * process by reffing the bonobo object and only unreffing it at idle
	 * time. If the bonobo object is being destroyed first, then don't do
	 * this because it exposes a bonobo bug.
	 */
	if (!GTK_OBJECT_DESTROYED (GTK_OBJECT (notes->view))) {
		bonobo_object_ref (BONOBO_OBJECT (notes->view));
		bonobo_object_idle_unref (BONOBO_OBJECT (notes->view));
        }
	
        done_with_file (notes);
        g_free (notes->uri);
        g_free (notes);

        notes_object_count--;
        if (notes_object_count <= 0) {
                gtk_main_quit();
        }
}

static BonoboObject *
make_notes_view (BonoboGenericFactory *Factory, const char *goad_id, gpointer closure)
{
        GtkWidget *vbox;
        Notes *notes;
        NautilusBackground *background;
        GdkFont *font;
         
        g_return_val_if_fail (strcmp (goad_id, "OAFIID:nautilus_notes_view:7f04c3cb-df79-4b9a-a577-38b19ccd4185") == 0, NULL);
        notes = g_new0 (Notes, 1);
        notes->uri = g_strdup ("");
        
        /* allocate a vbox to hold all of the UI elements */
        vbox = gtk_vbox_new (FALSE, 0);
        
        /* create the text container */               
        notes->note_text_field = gtk_text_new (NULL, NULL);
        
        font = nautilus_font_factory_get_font_from_preferences (14);
        nautilus_gtk_widget_set_font (notes->note_text_field, font);
        gdk_font_unref (font);

        gtk_text_set_editable (GTK_TEXT (notes->note_text_field), TRUE);	
        gtk_box_pack_start (GTK_BOX (vbox), notes->note_text_field, TRUE, TRUE, 0);
        background = nautilus_get_widget_background (notes->note_text_field);
        nautilus_background_set_color (background, NOTES_DEFAULT_BACKGROUND_COLOR);

	gtk_signal_connect (GTK_OBJECT (notes->note_text_field), "focus_out_event",
      	              	    GTK_SIGNAL_FUNC (on_text_field_focus_out_event),
                            notes);
     
        gtk_widget_show_all (vbox);
        
	/* Create CORBA object. */
        notes->view = nautilus_view_new (vbox);
        gtk_signal_connect (GTK_OBJECT (notes->view), "destroy", do_destroy, notes);

        notes_object_count++;
        
        /* handle events */
        gtk_signal_connect (GTK_OBJECT (notes->view), "load_location",
                            notes_load_location, notes);
        
        /* handle selections */
        nautilus_clipboard_set_up_editable_in_control
                (GTK_EDITABLE (notes->note_text_field),
                 nautilus_view_get_bonobo_control (notes->view),
                 FALSE);

/* FIXME bugzilla.eazel.com 4436: 
 * Undo not working in notes-view.
 */
#if 0
	nautilus_undo_set_up_editable_for_undo (GTK_EDITABLE (notes->note_text_field));
	nautilus_undo_editable_set_undo_key (GTK_EDITABLE (notes->note_text_field), TRUE);
#endif

        return BONOBO_OBJECT (notes->view);
}

int
main(int argc, char *argv[])
{
        BonoboGenericFactory *factory;
        CORBA_ORB orb;
        char *registration_id;

	/* Make criticals and warnings stop in the debugger if NAUTILUS_DEBUG is set.
	 * Unfortunately, this has to be done explicitly for each domain.
	 */
	if (g_getenv("NAUTILUS_DEBUG") != NULL) {
		nautilus_make_warnings_and_criticals_stop_in_debugger
			(G_LOG_DOMAIN, g_log_domain_glib, "Gdk", "Gtk", "GnomeVFS", "GnomeUI", "Bonobo", NULL);
	}
	
	/* Initialize gettext support */
#ifdef ENABLE_NLS /* sadly we need this ifdef because otherwise the following get empty statement warnings */
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
#endif
	
	/* Disable session manager connection */
	gnome_client_disable_master_connection ();
	
        /* initialize CORBA and Bonobo */
	gnomelib_register_popt_table (oaf_popt_options, oaf_get_popt_table_name ());
	orb = oaf_init (argc, argv);

        gnome_init ("nautilus-notes", VERSION,
                    argc, argv); 	
	gdk_rgb_init ();

        bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);
        
        /* initialize gnome-vfs, etc */
        g_thread_init (NULL);
        gnome_vfs_init ();
        
        registration_id = oaf_make_registration_id ("OAFIID:nautilus_notes_view_factory:4b39e388-3ca2-4d68-9f3d-c137ee62d5b0", getenv ("DISPLAY"));

        factory = bonobo_generic_factory_new_multi
                (registration_id,
                 make_notes_view, NULL);

        g_free (registration_id);

        do {
                bonobo_main();
        } while (notes_object_count > 0);
        
        gnome_vfs_shutdown ();
        return EXIT_SUCCESS;
}
