/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 2000, 2001 Eazel, Inc.
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

/* notes sidebar panel -- allows editing per-directory notes */

#include <config.h>

#include <eel/eel-background.h>
#include <eel/eel-debug.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-string.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktextbuffer.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkvbox.h>
#include <bonobo/bonobo-property-bag.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus/nautilus-clipboard.h>
#include <libnautilus/nautilus-view-standard-main.h>

/* FIXME bugzilla.gnome.org 44436: 
 * Undo not working in notes-view.
 */
#if 0
#include <libnautilus-private/nautilus-undo-signal-handlers.h>
#endif

#define NOTES_DEFAULT_BACKGROUND_COLOR "#FFFFBB"

#define SAVE_TIMEOUT (3 * 1000)

/* property bag getting and setting routines */
enum {
	TAB_IMAGE,
};

typedef struct {
	NautilusView *view;
	BonoboPropertyBag *property_bag;
	GtkWidget *note_text_field;
	GtkTextBuffer *text_buffer;
	char *uri;
	NautilusFile *file;
	guint save_timeout_id;
	char *previous_saved_text;
} Notes;

static void  notes_save_metainfo         (Notes      *notes);
static char *notes_get_indicator_image   (const char *notes_text);
static void  notify_listeners_if_changed (Notes      *notes,
                                          char       *new_notes);

static void
get_bonobo_properties (BonoboPropertyBag *bag,
			BonoboArg *arg,
			guint arg_id,
			CORBA_Environment *ev,
			gpointer callback_data)
{
        char *indicator_image;
        Notes *notes;

	notes = (Notes *) callback_data;
	
	switch (arg_id) {
        case TAB_IMAGE:	{
                /* if there is a note, return the name of the indicator image,
                   otherwise, return NULL */
                indicator_image = notes_get_indicator_image (notes->previous_saved_text);
                BONOBO_ARG_SET_STRING (arg, indicator_image);					
                g_free (indicator_image);
                break;
        }
        
        default:
                g_warning ("Unhandled arg %d", arg_id);
                break;
	}
}

static void
set_bonobo_properties (BonoboPropertyBag *bag,
			const BonoboArg *arg,
			guint arg_id,
			CORBA_Environment *ev,
			gpointer callback_data)
{
	g_warning ("Can't set note property %u", arg_id);
}

static gboolean
schedule_save_callback (gpointer data)
{
	Notes *notes;

	notes = data;

	/* Zero out save_timeout_id so no one will try to cancel our
	 * in-progress timeout callback.
         */
	notes->save_timeout_id = 0;
	
	notes_save_metainfo (notes);
	
	return FALSE;
}

static void
cancel_pending_save (Notes *notes)
{
	if (notes->save_timeout_id != 0) {
		gtk_timeout_remove (notes->save_timeout_id);
		notes->save_timeout_id = 0;
	}
}

static void
schedule_save (Notes *notes)
{
	cancel_pending_save (notes);
	
	notes->save_timeout_id = gtk_timeout_add (SAVE_TIMEOUT, schedule_save_callback, notes);
}

static void
load_note_text_from_metadata (NautilusFile *file,
			      Notes *notes)
{
        char *saved_text;

        g_assert (NAUTILUS_IS_FILE (file));
        g_assert (notes->file == file);

        saved_text = nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_ANNOTATION, "");

	/* This fn is called for any change signal on the file, so make sure that the
	 * metadata has actually changed.
	 */
        if (eel_strcmp (saved_text, notes->previous_saved_text) != 0) {
		notify_listeners_if_changed (notes, saved_text);
		
		g_free (notes->previous_saved_text);
        	notes->previous_saved_text = saved_text;
        	cancel_pending_save (notes);
        
		gtk_text_buffer_set_text (notes->text_buffer, saved_text, -1);
	} else {
		g_free (saved_text);
	}
	
/* FIXME bugzilla.gnome.org 44436: 
 * Undo not working in notes-view.
 */
#if 0
	nautilus_undo_set_up_editable_for_undo (GTK_EDITABLE (notes->note_text_field));
#endif
}

static void
done_with_file (Notes *notes)
{
	cancel_pending_save (notes);
	
	if (notes->file != NULL) {
		nautilus_file_monitor_remove (notes->file, notes);
		g_signal_handlers_disconnect_matched (notes->file,
                                                      G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                                                      0, 0, NULL,
                                                      G_CALLBACK (load_note_text_from_metadata),
                                                      notes);
	        nautilus_file_unref (notes->file);
        }
}

static void
notes_load_metainfo (Notes *notes)
{
        GList *attributes;

        done_with_file (notes);
        notes->file = nautilus_file_get (notes->uri);

	gtk_text_buffer_set_text (notes->text_buffer, "", -1);

        if (notes->file == NULL) {
		return;
        }

        attributes = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_METADATA);
        nautilus_file_monitor_add (notes->file, notes, attributes);

	if (nautilus_file_check_if_ready (notes->file, attributes)) {
		load_note_text_from_metadata (notes->file, notes);
	}
	
        g_list_free (attributes);
        
	g_signal_connect (notes->file,
			    "changed",
			    G_CALLBACK (load_note_text_from_metadata),
			    notes);
}

/* utility to notify event listeners if the notes data actually changed */
static void
notify_listeners_if_changed (Notes *notes, char *new_notes)
{
	char *tab_image;
	BonoboArg *tab_image_arg;

	if (eel_strcmp (notes->previous_saved_text, new_notes) != 0) {
		/* notify listeners that the notes text has changed */	
		tab_image = notes_get_indicator_image (new_notes);	
		
		tab_image_arg = bonobo_arg_new (BONOBO_ARG_STRING);
		BONOBO_ARG_SET_STRING (tab_image_arg, tab_image);			
#ifdef GNOME2_CONVERSION_COMPLETE 
		bonobo_property_bag_notify_listeners (notes->property_bag,
                                                      "tab_image", tab_image_arg, NULL);
#endif 
		bonobo_arg_release (tab_image_arg);
		g_free (tab_image);
	}
}

/* save the metainfo corresponding to the current uri, if any, into the text field */
static void
notes_save_metainfo (Notes *notes)
{
        char *notes_text;
	GtkTextIter start_iter;
	GtkTextIter end_iter;

        if (notes->file == NULL) {
                return;
        }

	cancel_pending_save (notes);
	
        /* Block the handler, so we don't respond to our own change.
         */
        g_signal_handlers_block_matched (notes->file,
                                         G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL,
                                         G_CALLBACK (load_note_text_from_metadata), 
                                         notes);

	gtk_text_buffer_get_start_iter (notes->text_buffer, &start_iter);
	gtk_text_buffer_get_end_iter (notes->text_buffer, &end_iter);
	notes_text = gtk_text_buffer_get_text (notes->text_buffer, 
					       &start_iter,
					       &end_iter,
					       FALSE);

	nautilus_file_set_metadata (notes->file,
                                    NAUTILUS_METADATA_KEY_ANNOTATION,
                                    NULL, notes_text);

        g_signal_handlers_unblock_matched (notes->file,
                                           G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                                           0, 0, NULL,
                                           G_CALLBACK (load_note_text_from_metadata), 
                                           notes);
	
	notify_listeners_if_changed (notes, notes_text);
	
	g_free (notes->previous_saved_text);
	notes->previous_saved_text = notes_text;        
}

static void
notes_load_location (NautilusView *view,
                     const char *location,
                     Notes *notes)
{
        if (strcmp (notes->uri, location) != 0) {
/* FIXME bugzilla.gnome.org 44436: 
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
			       gpointer callback_data)
{
	Notes *notes;

        notes = callback_data;
	notes_save_metainfo (notes);
	return FALSE;
}

static void
on_changed (GtkEditable *editable, Notes *notes)
{
	schedule_save (notes);
}

static void
do_destroy (GtkObject *obj, Notes *notes)
{
#ifdef GNOME2_CONVERSION_COMPLETE
	/* If the widget is being destroyed first, make sure the bonobo object
	 * that owns it is not destroyed half-way through the widget destroy
	 * process by reffing the bonobo object and only unreffing it at idle
	 * time. If the bonobo object is being destroyed first, then don't do
	 * this because it exposes a bonobo bug.
	 */
	if (!GTK_OBJECT_DESTROYED (GTK_OBJECT (notes->view))) {
		bonobo_object_ref (notes->view);
		bonobo_object_idle_unref (notes->view);
        }
#endif
	
        done_with_file (notes);
        g_free (notes->uri);
        g_free (notes->previous_saved_text);
        g_free (notes);
}

static char *
notes_get_indicator_image (const char *notes_text)
{
	if (notes_text != NULL && notes_text[0] != '\0') {
		return g_strdup ("note-indicator.png");
	}
	return NULL;
}

static NautilusView *
make_notes_view (const char *iid, gpointer callback_data)
{
        GtkWidget *vbox;
        Notes *notes;
        EelBackground *background;
        notes = g_new0 (Notes, 1);
        notes->uri = g_strdup ("");
        
        /* allocate a vbox to hold all of the UI elements */
        vbox = gtk_vbox_new (FALSE, 0);
       
        /* create the text container */               
	notes->text_buffer = gtk_text_buffer_new (NULL);
        notes->note_text_field = gtk_text_view_new_with_buffer (notes->text_buffer);
      
#ifdef GNOME2_CONVERSION_COMPLETE
        font = nautilus_font_factory_get_font_from_preferences (14);
        eel_gtk_widget_set_font (notes->note_text_field, font);
        gdk_font_unref (font);
#endif
	gtk_text_view_set_editable (GTK_TEXT_VIEW (notes->note_text_field), TRUE);	
        gtk_box_pack_start (GTK_BOX (vbox), notes->note_text_field, TRUE, TRUE, 0);

        background = eel_get_widget_background (notes->note_text_field);
        eel_background_set_color (background, NOTES_DEFAULT_BACKGROUND_COLOR);

	g_signal_connect (notes->note_text_field, "focus_out_event",
                          G_CALLBACK (on_text_field_focus_out_event),
                          notes);
	g_signal_connect (notes->text_buffer, "changed",
                          G_CALLBACK (on_changed),
                          notes);
     
        gtk_widget_show_all (vbox);
        
	/* Create CORBA object. */
        notes->view = nautilus_view_new (vbox);
        g_signal_connect (notes->view, "destroy", G_CALLBACK (do_destroy), notes);

	/* allocate a property bag to reflect the TAB_IMAGE property */
	notes->property_bag = bonobo_property_bag_new (get_bonobo_properties,  set_bonobo_properties, notes);
#ifdef GNOME2_CONVERSION_COMPLETE	
	bonobo_control_set_properties (nautilus_view_get_bonobo_control (notes->view), notes->property_bag);
	bonobo_property_bag_add (notes->property_bag, "tab_image", TAB_IMAGE, BONOBO_ARG_STRING, NULL,
				 "image indicating that a note is present", 0);
#endif
        /* handle events */
        g_signal_connect (notes->view, "load_location",
                            G_CALLBACK (notes_load_location), notes);
        
        /* handle selections */
#ifdef GNOME2_CONVERSION_COMPLETE
        nautilus_clipboard_set_up_editable_in_control
                (GTK_EDITABLE (notes->note_text_field),
                 nautilus_view_get_bonobo_control (notes->view),
                 FALSE);
#endif

/* FIXME bugzilla.gnome.org 44436: 
 * Undo not working in notes-view.
 */
#if 0
	nautilus_undo_set_up_editable_for_undo (GTK_EDITABLE (notes->note_text_field));
	nautilus_undo_editable_set_undo_key (GTK_EDITABLE (notes->note_text_field), TRUE);
#endif

        return notes->view;
}

int
main(int argc, char *argv[])
{
	if (g_getenv ("NAUTILUS_DEBUG") != NULL) {
		eel_make_warnings_and_criticals_stop_in_debugger ();
	}
	
        return nautilus_view_standard_main ("nautilus-notes",
                                            VERSION,
                                            GETTEXT_PACKAGE,
                                            GNOMELOCALEDIR,
                                            argc,
                                            argv,
                                            "OAFIID:nautilus_notes_view_factory:4b39e388-3ca2-4d68-9f3d-c137ee62d5b0",
                                            "OAFIID:nautilus_notes_view:7f04c3cb-df79-4b9a-a577-38b19ccd4185",
                                            make_notes_view,
                                            nautilus_global_preferences_init,
                                            NULL);
}
