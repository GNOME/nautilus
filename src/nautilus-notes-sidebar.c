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

#include "nautilus-notes-sidebar.h"

#include <eel/eel-debug.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-string.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktextbuffer.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkscrolledwindow.h>
#include <bonobo/bonobo-property-bag.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-clipboard.h>
#include <libnautilus-private/nautilus-bonobo-extensions.h>
#include <libnautilus-private/nautilus-sidebar-factory.h>
#include <libnautilus-private/nautilus-theme.h>

#define SAVE_TIMEOUT (3 * 1000)

static void load_note_text_from_metadata      (NautilusFile         *file,
                                               NautilusNotesSidebar *notes);
static void notes_save_metainfo               (NautilusNotesSidebar *notes);
static void nautilus_notes_sidebar_iface_init (NautilusSidebarIface *iface);
static void on_changed                        (GtkEditable          *editable,
                                               NautilusNotesSidebar *notes);

typedef struct {
	GtkScrolledWindowClass parent;
} NautilusNotesSidebarClass;

G_DEFINE_TYPE_WITH_CODE (NautilusNotesSidebar, nautilus_notes_sidebar, GTK_TYPE_SCROLLED_WINDOW,
			 G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_SIDEBAR,
						nautilus_notes_sidebar_iface_init));


struct _NautilusNotesSidebarDetails {
	NautilusWindowInfo *window;
	GtkWidget *note_text_field;
	GtkTextBuffer *text_buffer;
	char *uri;
	NautilusFile *file;
	guint save_timeout_id;
	char *previous_saved_text;
        GdkPixbuf *icon;
};

static gboolean
schedule_save_callback (gpointer data)
{
	NautilusNotesSidebar *notes;

	notes = data;

	/* Zero out save_timeout_id so no one will try to cancel our
	 * in-progress timeout callback.
         */
	notes->details->save_timeout_id = 0;
	
	notes_save_metainfo (notes);
	
	return FALSE;
}

static void
cancel_pending_save (NautilusNotesSidebar *notes)
{
	if (notes->details->save_timeout_id != 0) {
		g_source_remove (notes->details->save_timeout_id);
		notes->details->save_timeout_id = 0;
	}
}

static void
schedule_save (NautilusNotesSidebar *notes)
{
	cancel_pending_save (notes);
	
	notes->details->save_timeout_id = g_timeout_add (SAVE_TIMEOUT, schedule_save_callback, notes);
}

/* notifies event listeners if the notes data actually changed */
static void
set_saved_text (NautilusNotesSidebar *notes, char *new_notes)
{
        char *old_text;

	old_text = notes->details->previous_saved_text;
	notes->details->previous_saved_text = new_notes;        
        
	if (eel_strcmp (old_text, new_notes) != 0) {
                g_signal_emit_by_name (NAUTILUS_SIDEBAR (notes),
                                       "tab_icon_changed");
	}

        g_free (old_text);
}

/* save the metainfo corresponding to the current uri, if any, into the text field */
static void
notes_save_metainfo (NautilusNotesSidebar *notes)
{
        char *notes_text;
	GtkTextIter start_iter;
	GtkTextIter end_iter;

        if (notes->details->file == NULL) {
                return;
        }

	cancel_pending_save (notes);

        /* Block the handler, so we don't respond to our own change.
         */
        g_signal_handlers_block_matched (notes->details->file,
                                         G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL,
                                         G_CALLBACK (load_note_text_from_metadata), 
                                         notes);

	gtk_text_buffer_get_start_iter (notes->details->text_buffer, &start_iter);
	gtk_text_buffer_get_end_iter (notes->details->text_buffer, &end_iter);
	notes_text = gtk_text_buffer_get_text (notes->details->text_buffer, 
					       &start_iter,
					       &end_iter,
					       FALSE);

	nautilus_file_set_metadata (notes->details->file,
                                    NAUTILUS_METADATA_KEY_ANNOTATION,
                                    NULL, notes_text);

        g_signal_handlers_unblock_matched (notes->details->file,
                                           G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                                           0, 0, NULL,
                                           G_CALLBACK (load_note_text_from_metadata), 
                                           notes);
	
	set_saved_text (notes, notes_text);
}

static void
load_note_text_from_metadata (NautilusFile *file,
			      NautilusNotesSidebar *notes)
{
        char *saved_text;

        g_assert (NAUTILUS_IS_FILE (file));
        g_assert (notes->details->file == file);

        saved_text = nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_ANNOTATION, "");

	/* This fn is called for any change signal on the file, so make sure that the
	 * metadata has actually changed.
	 */
        if (eel_strcmp (saved_text, notes->details->previous_saved_text) != 0) {
                set_saved_text (notes, saved_text);
        	cancel_pending_save (notes);
        
                /* Block the handler, so we don't respond to our own change.
                 */
                g_signal_handlers_block_matched (notes->details->text_buffer,
                                                 G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                                                 0, 0, NULL,
                                                 G_CALLBACK (on_changed), 
                                                 notes);
		gtk_text_buffer_set_text (notes->details->text_buffer, saved_text, -1);
                g_signal_handlers_unblock_matched (notes->details->text_buffer,
                                                   G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                                                   0, 0, NULL,
                                                   G_CALLBACK (on_changed), 
                                                   notes);
	} else {
		g_free (saved_text);
	}
}

static void
done_with_file (NautilusNotesSidebar *notes)
{
	cancel_pending_save (notes);
	
	if (notes->details->file != NULL) {
		nautilus_file_monitor_remove (notes->details->file, notes);
		g_signal_handlers_disconnect_matched (notes->details->file,
                                                      G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                                                      0, 0, NULL,
                                                      G_CALLBACK (load_note_text_from_metadata),
                                                      notes);
	        nautilus_file_unref (notes->details->file);
        }
}

static void
notes_load_metainfo (NautilusNotesSidebar *notes)
{
        NautilusFileAttributes attributes;
        
        done_with_file (notes);
        notes->details->file = nautilus_file_get (notes->details->uri);

        /* Block the handler, so we don't respond to our own change.
         */
        g_signal_handlers_block_matched (notes->details->text_buffer,
                                         G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL,
                                         G_CALLBACK (on_changed), 
                                         notes);
	gtk_text_buffer_set_text (notes->details->text_buffer, "", -1);
        g_signal_handlers_unblock_matched (notes->details->text_buffer,
                                           G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                                           0, 0, NULL,
                                           G_CALLBACK (on_changed), 
                                           notes);

        if (notes->details->file == NULL) {
		return;
        }

        attributes = NAUTILUS_FILE_ATTRIBUTE_METADATA;
        nautilus_file_monitor_add (notes->details->file, notes, attributes);

	if (nautilus_file_check_if_ready (notes->details->file, attributes)) {
		load_note_text_from_metadata (notes->details->file, notes);
	}
	
	g_signal_connect (notes->details->file, "changed",
                          G_CALLBACK (load_note_text_from_metadata), notes);
}

static void
loading_uri_callback (NautilusSidebar *sidebar,
                      const char *location,
                      NautilusNotesSidebar *notes)
{
        if (strcmp (notes->details->uri, location) != 0) {
                notes_save_metainfo (notes);
                g_free (notes->details->uri);
                notes->details->uri = g_strdup (location);
                notes_load_metainfo (notes);
        }
}

static gboolean
on_text_field_focus_out_event (GtkWidget *widget,
			       GdkEventFocus *event,
			       gpointer callback_data)
{
	NautilusNotesSidebar *notes;

        notes = callback_data;
	notes_save_metainfo (notes);
	return FALSE;
}

static void
on_changed (GtkEditable *editable, NautilusNotesSidebar *notes)
{
	schedule_save (notes);
}

static void
nautilus_notes_sidebar_init (NautilusNotesSidebar *sidebar)
{
        char *image_path;
        NautilusNotesSidebarDetails *details;
        
        details = g_new0 (NautilusNotesSidebarDetails, 1);
        sidebar->details = details;
        
        details->uri = g_strdup ("");

        image_path = nautilus_theme_get_image_path ("note-indicator.png");
        if (image_path) {
                details->icon = gdk_pixbuf_new_from_file (image_path, NULL);
                g_free (image_path);
        }

        /* create the text container */               
	details->text_buffer = gtk_text_buffer_new (NULL);
        details->note_text_field = gtk_text_view_new_with_buffer (details->text_buffer);
      
	gtk_text_view_set_editable (GTK_TEXT_VIEW (details->note_text_field), TRUE);	
        gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (details->note_text_field),
                                     GTK_WRAP_WORD);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sidebar),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sidebar),
                                             GTK_SHADOW_IN);
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (sidebar), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (sidebar), NULL);
	gtk_container_add (GTK_CONTAINER (sidebar), details->note_text_field);

	g_signal_connect (details->note_text_field, "focus_out_event",
                          G_CALLBACK (on_text_field_focus_out_event), sidebar);
	g_signal_connect (details->text_buffer, "changed",
                          G_CALLBACK (on_changed), sidebar);
     
        gtk_widget_show_all (GTK_WIDGET (sidebar));
        
        /* handle selections */
#ifdef GNOME2_CONVERSION_COMPLETE
        nautilus_clipboard_set_up_editable_in_control
                (GTK_EDITABLE (notes->note_text_field),
                 nautilus_view_get_bonobo_control (notes->view),
                 FALSE);
#endif
}

static void
nautilus_notes_sidebar_finalize (GObject *object)
{
	NautilusNotesSidebar *sidebar;
	
	sidebar = NAUTILUS_NOTES_SIDEBAR (object);

        done_with_file (sidebar);
        if (sidebar->details->icon != NULL) {
                g_object_unref (sidebar->details->icon);
        }
        g_free (sidebar->details->uri);
        g_free (sidebar->details->previous_saved_text);
        g_free (sidebar->details);
        
	G_OBJECT_CLASS (nautilus_notes_sidebar_parent_class)->finalize (object);
}


static void
nautilus_notes_sidebar_class_init (NautilusNotesSidebarClass *class)
{
	G_OBJECT_CLASS (class)->finalize = nautilus_notes_sidebar_finalize;
}

static const char *
nautilus_notes_sidebar_get_sidebar_id (NautilusSidebar *sidebar)
{
	return NAUTILUS_NOTES_SIDEBAR_ID;
}

static char *
nautilus_notes_sidebar_get_tab_label (NautilusSidebar *sidebar)
{
	return g_strdup (_("Notes"));
}

static GdkPixbuf *
nautilus_notes_sidebar_get_tab_icon (NautilusSidebar *sidebar)
{
	NautilusNotesSidebar *notes;

        notes = NAUTILUS_NOTES_SIDEBAR (sidebar);
        
        if (notes->details->previous_saved_text != NULL &&
            notes->details->previous_saved_text[0] != '\0') {
                return g_object_ref (notes->details->icon);
        }
        
        return NULL;
}

static void
nautilus_notes_sidebar_is_visible_changed (NautilusSidebar *sidebar,
					     gboolean         is_visible)
{
	/* Do nothing */
}

static void
nautilus_notes_sidebar_iface_init (NautilusSidebarIface *iface)
{
	iface->get_sidebar_id = nautilus_notes_sidebar_get_sidebar_id;
	iface->get_tab_label = nautilus_notes_sidebar_get_tab_label;
	iface->get_tab_icon = nautilus_notes_sidebar_get_tab_icon;
	iface->is_visible_changed = nautilus_notes_sidebar_is_visible_changed;
}

static void
nautilus_notes_sidebar_set_parent_window (NautilusNotesSidebar *sidebar,
                                          NautilusWindowInfo *window)
{
	sidebar->details->window = window;

	g_signal_connect_object (window, "loading_uri",
				 G_CALLBACK (loading_uri_callback), sidebar, 0);
        
        g_free (sidebar->details->uri);
        sidebar->details->uri = nautilus_window_info_get_current_location (window);
        notes_load_metainfo (sidebar);
}

static NautilusSidebar *
nautilus_notes_sidebar_create (NautilusWindowInfo *window)
{
	NautilusNotesSidebar *sidebar;
	
	sidebar = g_object_new (nautilus_notes_sidebar_get_type (), NULL);
	nautilus_notes_sidebar_set_parent_window (sidebar, window);
	g_object_ref (sidebar);
	gtk_object_sink (GTK_OBJECT (sidebar));

	return NAUTILUS_SIDEBAR (sidebar);
}

static NautilusSidebarInfo notes_sidebar = {
	NAUTILUS_NOTES_SIDEBAR_ID,
	nautilus_notes_sidebar_create,
};

void
nautilus_notes_sidebar_register (void)
{
	nautilus_sidebar_factory_register (&notes_sidebar);
}

