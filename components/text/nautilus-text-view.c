/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Copyright (C) 2000 Eazel, Inc.
 *  Copyright (C) 2002 Sun Microsystems Inc.
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
 *  Authors: Dave Camp <dave@ximian.com>
 *  based on component by Andy Hertzfeld <andy@eazel.com>
 *    
 *
 */

/* text view - display a text file */

#include <config.h>

#include <string.h>
#include <eel/eel-debug.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkscrolledwindow.h>
#include <libnautilus/nautilus-view.h>
#include <libnautilus/nautilus-view-standard-main.h>

#define FACTORY_IID "OAFIID:nautilus_text_view_factory:124ae209-d356-418f-8757-54e071cb3a21"
#define VIEW_IID "OAFIID:nautilus_text_view:fa466311-17c1-435c-8231-c9fc434b6437" 

typedef struct {
	NautilusView base;

        GtkTextBuffer *buffer;

        EelReadFileHandle *read_handle;
} NautilusTextView;

typedef struct {
	NautilusViewClass base;
} NautilusTextViewClass;

static const char *encodings_to_try[2];
static int n_encodings_to_try;

static GType nautilus_text_view_get_type (void);

BONOBO_CLASS_BOILERPLATE (NautilusTextView, nautilus_text_view,
                          NautilusView, NAUTILUS_TYPE_VIEW);

static void
unload_contents (NautilusTextView *view)
{
        GtkTextIter start;
        GtkTextIter end;

        gtk_text_buffer_get_start_iter (view->buffer, &start);
        gtk_text_buffer_get_end_iter (view->buffer, &end);
                        
        gtk_text_buffer_delete (view->buffer, &start, &end);
}

static void
cancel_load (NautilusTextView *view)
{
        if (view->read_handle) {
                eel_read_file_cancel (view->read_handle);
                view->read_handle = NULL;
        }
}

static GnomeVFSFileSize
my_strnlen(char *str, GnomeVFSFileSize file_size)
{
        GnomeVFSFileSize len;

        len = 0;
        while (*str != 0 && len < file_size) {
                str++;
                len++;
        }
        return len;
}

static void
read_file_callback (GnomeVFSResult result,
                    GnomeVFSFileSize file_size,
                    char *file_contents,
                    gpointer callback_data)
{
        NautilusView *view;
        NautilusTextView *text_view;
        GnomeVFSFileSize length;
        gsize converted_length;
        char *utf8_contents;
        GError *conversion_error;
        GtkTextIter iter;
        int i;

        view = callback_data;
        text_view = callback_data;

        text_view->read_handle = NULL;
        
        if (result != GNOME_VFS_OK) {
                nautilus_view_report_load_failed (view);
                return;
        }

        /* Find first embedded zero, if any */
        length = my_strnlen (file_contents, file_size);
        
        utf8_contents = NULL;
        if (!g_utf8_validate (file_contents, length, NULL)) {
                for (i = 0; i < n_encodings_to_try; i++) {
                        conversion_error = NULL;
                        utf8_contents = g_convert (file_contents, length, 
                                                   "UTF-8", encodings_to_try[i],
                                                   NULL, &converted_length, &conversion_error);
                        if (utf8_contents != NULL) {
                                length = converted_length;
                                break;
                        }
                        g_error_free (conversion_error);
                }
                
                if (utf8_contents == NULL) {
                        nautilus_view_report_load_failed (view);
                        return;
                }
                file_contents = utf8_contents;
        }
        
        gtk_text_buffer_get_start_iter (text_view->buffer, &iter);
        
        gtk_text_buffer_insert (text_view->buffer, &iter, 
                                file_contents, length);
        
        if (utf8_contents) {
                g_free (utf8_contents);
        }
        
        nautilus_view_report_load_complete (view);
}

static void
load_location (NautilusView *view, const char *location_uri)
{
        NautilusTextView *text_view;
        
        text_view = (NautilusTextView *)view;
        
        cancel_load (text_view);
        unload_contents (text_view);
        
        nautilus_view_report_load_underway (view);

        text_view->read_handle = 
                eel_read_entire_file_async (location_uri, 0, 
                                            read_file_callback, view);
}

static void
nautilus_text_view_instance_init (NautilusTextView *view)
{
        GtkWidget *text_view;
        GtkWidget *scrolled_window;
        
        text_view = gtk_text_view_new ();
        gtk_text_view_set_editable (GTK_TEXT_VIEW (text_view), FALSE);

        scrolled_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
        
        gtk_container_add (GTK_CONTAINER (scrolled_window), text_view);

        gtk_widget_show (text_view);
        gtk_widget_show (scrolled_window);

        view->buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));

        nautilus_view_construct (NAUTILUS_VIEW (view), scrolled_window);
}

static void
nautilus_text_view_destroy (GtkObject *object)
{
        NautilusTextView *view;
        
        view = (NautilusTextView*)object;        
                
        cancel_load (view);
}

static void
nautilus_text_view_class_init (NautilusTextViewClass *class)
{
        GtkObjectClass *object_class = GTK_OBJECT_CLASS (class);
        NautilusViewClass *view_class = NAUTILUS_VIEW_CLASS (class);
        const char *charset;
        gboolean utf8;

        view_class->load_location = load_location;

        object_class->destroy = nautilus_text_view_destroy;

        n_encodings_to_try = 0;
        utf8 = g_get_charset (&charset);
        
        if (!utf8) {
                encodings_to_try[n_encodings_to_try++] = charset;
        }
        
        if (g_ascii_strcasecmp (charset, "ISO-8859-1") != 0) {
                encodings_to_try[n_encodings_to_try++] = "ISO-8859-1";
        }
}

int
main (int argc, char *argv[])
{
        if (g_getenv ("NAUTILUS_DEBUG") != NULL) {
                eel_make_warnings_and_criticals_stop_in_debugger ();
        }

        return nautilus_view_standard_main ("nautilus-text-view",
                                            VERSION,
                                            GETTEXT_PACKAGE,
                                            GNOMELOCALEDIR,
                                            argc,
                                            argv,
                                            FACTORY_IID,
                                            VIEW_IID,
                                            nautilus_view_create_from_get_type_function,
                                            NULL,
                                            nautilus_text_view_get_type);
}



