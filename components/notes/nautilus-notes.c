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

#include <libnautilus/libnautilus.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <liboaf/liboaf.h>
#include <limits.h>
#include <ctype.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-debug.h>
#include <libnautilus/nautilus-clipboard.h>
#include <libnautilus/nautilus-undo-manager.h>
#include <libnautilus/nautilus-undo.h>
#define NOTES_DEFAULT_BACKGROUND_COLOR "rgb:FFFF/FFFF/BBBB"


typedef struct {
        NautilusView *view;
        GtkWidget *note_text_field;
        char *uri;
        NautilusFile *file;
} Notes;

static int notes_object_count = 0;

static void
finish_loading_note (NautilusFile *file,
                     gpointer callback_data)
{
        Notes *notes;
        int position;
        char *notes_text;

        g_assert (NAUTILUS_IS_FILE (file));

        notes = callback_data;
        g_assert (notes->file == file);

        notes_text = nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_ANNOTATION, "");
        position = 0;
        gtk_editable_insert_text (GTK_EDITABLE (notes->note_text_field),
                                  notes_text,
                                  strlen (notes_text),
                                  &position);
        g_free (notes_text);
}

static void
done_with_file (Notes *notes)
{
        nautilus_file_cancel_callback (notes->file, finish_loading_note, notes);
        nautilus_file_unref (notes->file);
}

static void
notes_load_metainfo (Notes *notes)
{
        gtk_editable_delete_text (GTK_EDITABLE (notes->note_text_field), 0, -1);   
        
        done_with_file (notes);
        notes->file = nautilus_file_get (notes->uri);
        if (notes->file == NULL) {
                return;
        }
        nautilus_file_call_when_ready (notes->file, NULL, TRUE, finish_loading_note, notes);
}

/* save the metainfo corresponding to the current uri, if any, into the text field */

static void
notes_save_metainfo (Notes *notes)
{
        char *notes_text;

        if (notes->file == NULL) {
                return;
        }

        notes_text = gtk_editable_get_chars (GTK_EDITABLE (notes->note_text_field), 0 , -1);
        nautilus_file_set_metadata (notes->file, NAUTILUS_METADATA_KEY_ANNOTATION, NULL, notes_text);
        g_free (notes_text);
}

static void
notes_notify_location_change (NautilusView *view,
                              Nautilus_NavigationInfo *loci,
                              Notes *notes)
{
        if (strcmp (notes->uri, loci->requested_uri) != 0) {
                notes_save_metainfo (notes);
                g_free (notes->uri);
                notes->uri = g_strdup (loci->requested_uri);
                notes_load_metainfo (notes);
        }

#if 0
	{
		CORBA_Environment ev;
		Nautilus_Undo_Manager undo_manager;
		Nautilus_Undo_Transaction undo_transaction;
		NautilusUndoTransaction *transaction;
		BonoboObject *view_control;
		
		CORBA_exception_init(&ev);

		view_control = nautilus_view_get_bonobo_control (view);
		g_assert (view_control);
		
		undo_manager = bonobo_object_query_interface (view_control, 
      						"IDL:Nautilus/Undo/Context:1.0");
      		g_assert (undo_manager);

		transaction = nautilus_undo_transaction_new ("Test");
		g_assert (transaction);

		undo_transaction = bonobo_object_corba_objref (BONOBO_OBJECT (transaction));
		g_assert (undo_transaction);
		
   		Nautilus_Undo_Manager_append (undo_manager, undo_transaction, &ev);
				      
		CORBA_exception_free(&ev);	
	}
#endif

}


                              
static void
do_destroy (GtkObject *obj, Notes *notes)
{
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
        NautilusClipboardInfo *info;
         
        g_return_val_if_fail (strcmp (goad_id, "OAFIID:ntl_notes_view:7f04c3cb-df79-4b9a-a577-38b19ccd4185") == 0, NULL);
        notes = g_new0 (Notes, 1);
        notes->uri = g_strdup ("");
        
        /* allocate a vbox to hold all of the UI elements */
        
        vbox = gtk_vbox_new (FALSE, 0);
        
        /* create the text container */
        
        notes->note_text_field = gtk_text_new (NULL, NULL);
        gtk_text_set_editable (GTK_TEXT (notes->note_text_field), TRUE);
        gtk_box_pack_start (GTK_BOX (vbox), notes->note_text_field, TRUE, TRUE, 0);
        background = nautilus_get_widget_background (GTK_WIDGET (notes->note_text_field));
        nautilus_background_set_color (background, NOTES_DEFAULT_BACKGROUND_COLOR);

        gtk_widget_show_all (vbox);
        
        /* Create CORBA object. */
        notes->view = NAUTILUS_VIEW (nautilus_meta_view_new (vbox));
        gtk_signal_connect (GTK_OBJECT (notes->view), "destroy", do_destroy, notes);

        notes_object_count++;
        
        /* handle events */
        gtk_signal_connect (GTK_OBJECT (notes->view), "notify_location_change",
                            notes_notify_location_change, notes);
        
        /* handle selections */
        info = nautilus_clipboard_info_new ();
	nautilus_clipboard_info_set_view (info, notes->view);
	nautilus_clipboard_info_set_clipboard_owner (info, GTK_WIDGET (notes->note_text_field));
	nautilus_clipboard_info_set_component_name (info, _("Notes"));
        gtk_signal_connect (GTK_OBJECT (notes->note_text_field), "focus_in_event",
                            GTK_SIGNAL_FUNC (nautilus_component_merge_bonobo_items_cb), info); 
	gtk_signal_connect (GTK_OBJECT (notes->note_text_field), "focus_out_event",
			    GTK_SIGNAL_FUNC (nautilus_component_unmerge_bonobo_items_cb), info); 

        gtk_signal_connect (GTK_OBJECT (notes->view), "destroy", nautilus_clipboard_info_destroy, info);
        /* set description */

        return BONOBO_OBJECT (notes->view);        
}

int
main(int argc, char *argv[])
{
        BonoboGenericFactory *factory;
        CORBA_ORB orb;

	/* Make criticals and warnings stop in the debugger if NAUTILUS_DEBUG is set.
	 * Unfortunately, this has to be done explicitly for each domain.
	 */
	if (getenv("NAUTILUS_DEBUG") != NULL) {
		nautilus_make_warnings_and_criticals_stop_in_debugger
			(G_LOG_DOMAIN, g_log_domain_glib, "Gdk", "Gtk", "GnomeVFS", "GnomeUI", "Bonobo", NULL);
	}
	
	/* Initialize gettext support */
#ifdef ENABLE_NLS /* sadly we need this ifdef because otherwise the following get empty statement warnings */
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
#endif
	
        /* initialize CORBA and Bonobo */

        gnome_init_with_popt_table("ntl-notes", VERSION,
				   argc, argv,
				   oaf_popt_options, 0, NULL); 
	orb = oaf_init (argc, argv);

        bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);
        
        /* initialize gnome-vfs, etc */
        g_thread_init (NULL);
        gnome_vfs_init ();
        
        factory = bonobo_generic_factory_new_multi ("OAFIID:ntl_notes_view_factory:4b39e388-3ca2-4d68-9f3d-c137ee62d5b0",
                                                    make_notes_view, NULL);

        do {
                bonobo_main();
        } while (notes_object_count > 0);
        
        return EXIT_SUCCESS;
}
