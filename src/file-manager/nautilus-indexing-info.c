/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  nautilus-indexing-info.c: Indexing Info for the search service
 *
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: George Lebl <jirka@5z.com>
 *           Rebecca Schulman <rebecka@eazel.com>
 */

#include <config.h>
#include "nautilus-indexing-info.h"

#include <gtk/gtkhbox.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-preferences.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>

#ifdef HAVE_MEDUSA

#include <libmedusa/medusa-index-service.h>
#include <libmedusa/medusa-index-progress.h>
#include <libmedusa/medusa-indexed-search.h>
#include <libmedusa/medusa-system-state.h>

#define PROGRESS_UPDATE_INTERVAL 5000

typedef struct {
        NautilusLabel *progress_label;
        GtkProgress *progress_bar;
} ProgressChangeData;

typedef struct {
        GnomeDialog *last_index_time_dialog;
        MedusaIndexingStatus indexing_service_status;
        GnomeDialog *index_in_progress_dialog;
        gboolean show_last_index_time;
} IndexingInfoDialogs;

static IndexingInfoDialogs *dialogs = NULL;

static GnomeDialog *   last_index_time_and_reindex_button_dialog_new      (void);

static int
get_index_percentage_complete (void)
{
        return medusa_index_progress_get_percentage_complete ();
}

static void
initialize_dialog (GnomeDialog *dialog)
{
        gnome_dialog_set_close (dialog, TRUE /*click_closes*/);
        gnome_dialog_close_hides (dialog, TRUE /*just_hide*/);

}

static char *
get_text_for_progress_label (void)
{
        return g_strdup_printf (_("Indexing is %d%% complete."), get_index_percentage_complete ());
}


static gboolean
update_progress_display (gpointer callback_data)
{
        NautilusLabel *progress_label;
        ProgressChangeData *progress_change_data;
        char *progress_string;

        progress_change_data = (ProgressChangeData *) callback_data;

        progress_label = NAUTILUS_LABEL (progress_change_data->progress_label);
        progress_string = get_text_for_progress_label ();
        nautilus_label_set_text (progress_label, progress_string);
        g_free (progress_string);
        gtk_progress_set_value (progress_change_data->progress_bar, get_index_percentage_complete ());


        return TRUE;
}

static void
dialog_close_cover (gpointer dialog_data)
{
        g_assert (GNOME_IS_DIALOG (dialog_data));

        gnome_dialog_close (dialog_data);
}


static void
show_index_progress_dialog (void)
{
        int callback_id;
        
        gnome_dialog_close (dialogs->last_index_time_dialog);
        gtk_widget_show_all (GTK_WIDGET (dialogs->index_in_progress_dialog));
        callback_id = medusa_execute_once_when_system_state_changes (MEDUSA_SYSTEM_STATE_DISABLED | MEDUSA_SYSTEM_STATE_BLOCKED,
                                                                     dialog_close_cover,
                                                                     dialogs->index_in_progress_dialog);
        gtk_signal_connect_object (GTK_OBJECT (dialogs->index_in_progress_dialog),
                                   "destroy",
                                   medusa_remove_state_changed_function,
                                   GINT_TO_POINTER (callback_id));
}



static void
show_reindex_request_dialog (void)
{
        int callback_id;

        gnome_dialog_close (dialogs->index_in_progress_dialog);
        gtk_widget_show_all (GTK_WIDGET (dialogs->last_index_time_dialog));
        callback_id = medusa_execute_once_when_system_state_changes (MEDUSA_SYSTEM_STATE_DISABLED | MEDUSA_SYSTEM_STATE_BLOCKED,
                                                                     dialog_close_cover,
                                                                     dialogs->last_index_time_dialog);
        gtk_signal_connect_object (GTK_OBJECT (dialogs->last_index_time_dialog),
                                   "destroy",
                                   medusa_remove_state_changed_function,
                                   GINT_TO_POINTER (callback_id));
}

static void
recreate_and_show_reindex_request_dialog (void)
{
        gnome_dialog_close (dialogs->last_index_time_dialog);
        gtk_widget_destroy (GTK_WIDGET (dialogs->last_index_time_dialog));
        dialogs->last_index_time_dialog = last_index_time_and_reindex_button_dialog_new ();
        gtk_widget_show_all (GTK_WIDGET (dialogs->last_index_time_dialog));
}
        
static void
update_file_index_callback (GtkWidget *widget, gpointer data)
{
	MedusaIndexingRequestResult result;
	const char *error;

	result = medusa_index_service_request_reindex ();
        
	switch (result) {
	case MEDUSA_INDEXER_REQUEST_OK:
		error = NULL;
                dialogs->show_last_index_time = FALSE;
                show_index_progress_dialog ();
		break;
	case MEDUSA_INDEXER_ERROR_BUSY:
		error = _("The indexer is currently busy.");
		break;
	case MEDUSA_INDEXER_ERROR_NO_RESPONSE:
                dialogs->indexing_service_status = MEDUSA_INDEXER_NOT_AVAILABLE;
                recreate_and_show_reindex_request_dialog ();
		error = _("An indexer is not running, or is not responding to requests to reindex your computer.");
		break;
	case MEDUSA_INDEXER_ERROR_INVALID_RESPONSE:
                dialogs->indexing_service_status = MEDUSA_INDEXER_NOT_AVAILABLE;
                recreate_and_show_reindex_request_dialog ();
                error = _("An attempt to reindex, caused an Internal Indexer Error.  Tell rebecka@eazel.com");
                break;
        default:
                g_assert_not_reached ();
                error = NULL;
	}
        if (error) {
                /* FIXME: Maybe we should include details here? */
                nautilus_show_error_dialog (error,
                                            _("Reindexing Failed"),
                                            NULL);
        }
        
}

static GnomeDialog *
last_index_time_and_reindex_button_dialog_new (void)
{
	char *time_str;
	char *label_str;
        GtkWidget *label;
        GtkWidget *button;
        GtkWidget *hbox;
        GnomeDialog *dialog;

        dialogs->indexing_service_status = medusa_index_service_is_available ();
        switch (dialogs->indexing_service_status) {
        case MEDUSA_INDEXER_ALREADY_INDEXING:
                /* Fall through, we won't be showing this in the above case, anyways. */
        case MEDUSA_INDEXER_READY_TO_INDEX:
                
                dialog = nautilus_create_info_dialog (_("Once a day your files and text content are indexed so "
                                                        "your searches are fast. If you need to update your index "
                                                        "now, click on the \"Update Now\" button."),
                                                      _("Indexing Status"),
                                                      NULL);
                break;
        case MEDUSA_INDEXER_NOT_AVAILABLE:
                /* FIXME: Do we want to talk about the index not being available? */
                dialog = nautilus_create_info_dialog (_("Once a day your files and text content are indexed so "
                                                        "your searches are fast. "),
                                                      _("Indexing Status"),
                                                      NULL);
                break;
        default:
                g_assert_not_reached ();
                dialog = NULL;
        }
        
        initialize_dialog (dialog);
        time_str = nautilus_indexing_info_get_last_index_time ();
        label_str = g_strdup_printf (_("Your files were last indexed at %s"),
                                     time_str);
        g_free (time_str);
        
        label = nautilus_label_new (label_str);
        nautilus_label_set_justify (NAUTILUS_LABEL (label), GTK_JUSTIFY_LEFT);
        nautilus_label_make_bold (NAUTILUS_LABEL (label));
        gtk_box_pack_start (GTK_BOX (dialog->vbox), label,
                            FALSE, FALSE, 0);

        if (dialogs->indexing_service_status == MEDUSA_INDEXER_READY_TO_INDEX) {
                hbox = gtk_hbox_new (FALSE, 0);
                button = gtk_button_new_with_label (_("Update Now"));
                gtk_signal_connect (GTK_OBJECT (button), "clicked",
                                    update_file_index_callback, NULL);
                
                gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
                gtk_box_pack_start (GTK_BOX (dialog->vbox), hbox,
                            FALSE, FALSE, 0);
        }

        return dialog;
}

static void
timeout_remove_callback (gpointer callback_data)
{
        gtk_timeout_remove (GPOINTER_TO_UINT (callback_data));
}

static GnomeDialog *
index_progress_dialog_new (void)
{
        GtkWidget *progress_label;
        GtkWidget *indexing_progress_bar;
        GtkWidget *progress_bar_hbox, *embedded_vbox;
        GnomeDialog *dialog;
        char *progress_string;
        int percentage_complete;
        ProgressChangeData *progress_data;
        guint timeout_id;
                
        dialog = nautilus_create_info_dialog (_("Once a day your files and text content are indexed so "
                                                "your searches are fast.  Your files are currently being indexed."),
                                              _("Indexing Status"), NULL);
        initialize_dialog (dialog);        
        percentage_complete = get_index_percentage_complete ();
        indexing_progress_bar = gtk_progress_bar_new ();

        embedded_vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);

        gtk_progress_set_show_text (GTK_PROGRESS (indexing_progress_bar), FALSE);
        gtk_progress_configure (GTK_PROGRESS (indexing_progress_bar), percentage_complete, 0, 100);
        /* Put the progress bar in an hbox to make it a more sane size */
        gtk_box_pack_start (GTK_BOX (embedded_vbox), indexing_progress_bar, FALSE, FALSE, 0);

        progress_string = get_text_for_progress_label ();
        progress_label = nautilus_label_new (progress_string);
        g_free (progress_string);
        nautilus_label_set_justify (NAUTILUS_LABEL (progress_label), GTK_JUSTIFY_LEFT);
        gtk_box_pack_start (GTK_BOX (embedded_vbox), progress_label, FALSE, FALSE, 0);

        progress_bar_hbox = gtk_hbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (progress_bar_hbox), embedded_vbox,
                            FALSE, FALSE, GNOME_PAD_SMALL);

        gtk_box_pack_start (GTK_BOX (dialog->vbox), progress_bar_hbox,
                            FALSE, FALSE, 0);

        /* Keep the dialog current with actual indexing progress */
        progress_data = g_new (ProgressChangeData, 1);
        progress_data->progress_label = NAUTILUS_LABEL (progress_label);
        progress_data->progress_bar = GTK_PROGRESS (indexing_progress_bar);
        timeout_id = gtk_timeout_add_full (PROGRESS_UPDATE_INTERVAL,
                                           update_progress_display,
                                           NULL,
                                           progress_data,
                                           g_free);
        gtk_signal_connect (GTK_OBJECT (progress_bar_hbox),
                            "destroy",
                            timeout_remove_callback,
                            GUINT_TO_POINTER (timeout_id));
        
        return dialog;
}



static void
destroy_indexing_info_dialogs_on_exit (void)
{
        gtk_widget_destroy (GTK_WIDGET (dialogs->index_in_progress_dialog));
        gtk_widget_destroy (GTK_WIDGET (dialogs->last_index_time_dialog));
        g_free (dialogs);
}


static void
show_indexing_info_dialog (void)
{
        GnomeDialog *dialog_shown;
        int callback_id;
        
        if (medusa_system_services_are_blocked ()) {
                dialog_shown = nautilus_show_info_dialog (_("To do a fast search, Find requires "
                                                            "an index of the files on your system. "
                                                            "Your system administrator has disabled "
                                                            "fast search on your computer, so no "
                                                            "index is available."),
                                                          _("Fast searches are not available on your computer."),
                                                          NULL);
                callback_id = medusa_execute_once_when_system_state_changes (MEDUSA_SYSTEM_STATE_ENABLED | MEDUSA_SYSTEM_STATE_DISABLED,
                                                                             dialog_close_cover,
                                                                             dialog_shown);
                gtk_signal_connect_object (GTK_OBJECT (dialog_shown),
                                           "destroy",
                                           medusa_remove_state_changed_function,
                                           GINT_TO_POINTER (callback_id));
                return;

        }

        if (!medusa_system_services_are_enabled ()) {
                dialog_shown = nautilus_show_info_dialog_with_details (_("To do a fast search, Find requires an index "
                                                                         "of the files on your system. Fast search is "
                                                                         "disabled in your Search preferences, so no "
                                                                         "index is available."),
                                                                       _("Fast searches are not available on your computer."),
                                                                       _("To enable fast search, open the Preferences menu "
                                                                         "and choose Preferences. Then select Search "
                                                                         "preferences and put a checkmark in the Enable "
                                                                         "Fast Search checkbox. An index will be generated "
                                                                         "while your computer is idle, so your index won't "
                                                                         "be available immediately."),
                                                                       NULL);
                
                callback_id = medusa_execute_once_when_system_state_changes (MEDUSA_SYSTEM_STATE_ENABLED | MEDUSA_SYSTEM_STATE_BLOCKED,
                                                                             dialog_close_cover,
                                                                             dialog_shown);
                gtk_signal_connect_object (GTK_OBJECT (dialog_shown),
                                           "destroy",
                                           medusa_remove_state_changed_function,
                                           GINT_TO_POINTER (callback_id));
                return;
        }
	if (dialogs == NULL) {
                dialogs = g_new0 (IndexingInfoDialogs, 1);
                g_atexit (destroy_indexing_info_dialogs_on_exit);
                
                dialogs->last_index_time_dialog = last_index_time_and_reindex_button_dialog_new ();
                dialogs->index_in_progress_dialog = index_progress_dialog_new ();
        }
 
       dialogs->show_last_index_time = !medusa_index_is_currently_running ();
       
       if (dialogs->show_last_index_time) {
               show_reindex_request_dialog ();
       } else {
               show_index_progress_dialog ();
       }
        
}

#endif /* HAVE_MEDUSA */

static void
show_search_service_not_available_dialog (void)
{
        nautilus_show_error_dialog (_("Sorry, but the medusa search service is not available."),
                                    _("Search Service Not Available"),
                                    NULL);
}


void
nautilus_indexing_info_request_reindex (void)
{
#ifdef HAVE_MEDUSA
        medusa_index_service_request_reindex ();
#endif
}

char *
nautilus_indexing_info_get_last_index_time (void)
{
#ifdef HAVE_MEDUSA
	time_t update_time;
        
	update_time = medusa_index_service_get_last_index_update_time ();
        if (update_time) {
                return nautilus_strdup_strftime (_("%I:%M %p, %x"),
                                                 localtime (&update_time));
        } else {
                return NULL;
        }
#else
        g_warning ("called nautilus_indexing_info_get_last_index_time with HAVE_MEDUSA off");
        return g_strdup ("");
#endif
}

/**
 * nautilus_indexing_info_show_dialog:
 *
 * Show the indexing info dialog.  If one is already
 * running, just raise that one.
 **/
void
nautilus_indexing_info_show_dialog (void)
{
#ifdef HAVE_MEDUSA
        if (medusa_indexed_search_system_index_files_look_available () ||
            medusa_index_service_is_available () == MEDUSA_INDEXER_ALREADY_INDEXING) {
                show_indexing_info_dialog ();
        }
        else {
                show_search_service_not_available_dialog ();
        }
#else
        show_search_service_not_available_dialog ();
#endif
}
