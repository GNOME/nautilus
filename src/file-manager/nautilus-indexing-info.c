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
 */

#include <config.h>
#include "nautilus-indexing-info.h"

#include <gtk/gtkhbox.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>

#ifdef HAVE_MEDUSA

#include <libmedusa/medusa-index-service.h>
#include <libmedusa/medusa-index-progress.h>
#include <libmedusa/medusa-indexed-search.h>

#define PROGRESS_UPDATE_INTERVAL 5000

typedef struct {
        NautilusLabel *progress_label;
        GtkProgress *progress_bar;
} ProgressChangeData;

static GtkWidget *indexing_info_dialog = NULL;

static char *
get_text_for_progress_label (void)
{
        return g_strdup_printf (_("Indexing is %d%% complete."),
                                medusa_index_progress_get_percentage_complete ());
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
        gtk_progress_set_value (progress_change_data->progress_bar,
                                medusa_index_progress_get_percentage_complete ());

        return TRUE;
}

static void
update_file_index_callback (GtkWidget *widget, gpointer data)
{
	MedusaIndexingRequestResult result;
	const char *error;

	result = medusa_index_service_request_reindex ();
        
	switch (result) {
	default:
	case MEDUSA_INDEXER_REQUEST_OK:
		error = NULL;
		break;
	case MEDUSA_INDEXER_ERROR_BUSY:
		error = _("Error while trying to reindex: Busy");
		break;
	case MEDUSA_INDEXER_ERROR_NO_RESPONSE:
		error = _("Error while trying to reindex: No response");
		break;
	case MEDUSA_INDEXER_ERROR_INVALID_RESPONSE:
                error = _("Error while trying to reindex: Internal Indexer Error.  Tell rebecka@eazel.com");
                break;
	}

	if (error != NULL) {
		nautilus_error_dialog (error, _("Error While Reindexing"), NULL);
	}
}

static void 
show_reindex_request_information (GnomeDialog *gnome_dialog)
{
	char *time_str;
	char *label_str;
        GtkWidget *label;
        GtkWidget *button;
        GtkWidget *hbox;
        
        time_str = nautilus_indexing_info_get_last_index_time ();
        label_str = g_strdup_printf (_("Your files were last indexed at %s"),
                                     time_str);
        g_free (time_str);
        
        label = nautilus_label_new (label_str);
        nautilus_label_set_justify (NAUTILUS_LABEL (label), GTK_JUSTIFY_LEFT);
        nautilus_label_make_bold (NAUTILUS_LABEL (label));
        gtk_box_pack_start (GTK_BOX (gnome_dialog->vbox), label,
                            FALSE, FALSE, 0);
        
        hbox = gtk_hbox_new (FALSE, 0);
        button = gtk_button_new_with_label (_("Update Now"));
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                            update_file_index_callback, NULL);

        gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (gnome_dialog->vbox), hbox,
                            FALSE, FALSE, 0);
}

static void
timeout_remove_callback (gpointer callback_data)
{
        gtk_timeout_remove (GPOINTER_TO_UINT (callback_data));
}

static void
show_index_progress_bar (GnomeDialog *gnome_dialog)
{
        GtkWidget *indexing_notification_label, *progress_label;
        GtkWidget *indexing_progress_bar;
        GtkWidget *progress_bar_hbox, *embedded_vbox;
        char *progress_string;
        int percentage_complete;
        ProgressChangeData *progress_data;
        guint timeout_id;
                
        indexing_notification_label = nautilus_label_new (_("Your files are currently being indexed:"));
        nautilus_label_make_bold (NAUTILUS_LABEL (indexing_notification_label));
        nautilus_label_set_justify (NAUTILUS_LABEL (indexing_notification_label), GTK_JUSTIFY_LEFT);
        gtk_box_pack_start (GTK_BOX (gnome_dialog->vbox), indexing_notification_label,
                            FALSE, FALSE, 0);
        
        percentage_complete = medusa_index_progress_get_percentage_complete ();
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

        gtk_box_pack_start (GTK_BOX (gnome_dialog->vbox), progress_bar_hbox,
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
}

static void
show_indexing_info_dialog (void)
{
	GnomeDialog *gnome_dialog;
        GtkWidget *label;

	/* A dialog is up already */
	if (indexing_info_dialog != NULL) {
		nautilus_gtk_window_present (GTK_WINDOW (indexing_info_dialog));
		return;
	}

        /* FIXME: Should set title, maybe use nautilus stock dialogs. */
	indexing_info_dialog = gnome_dialog_new (_("Indexing Info"),
                                                 GNOME_STOCK_BUTTON_OK,
                                                 NULL);

	gnome_dialog = GNOME_DIALOG (indexing_info_dialog);

	gnome_dialog_set_close (gnome_dialog, TRUE /*click_closes*/);
        /* FIXME: The dialog should be freed at exit */
	gnome_dialog_close_hides (gnome_dialog, TRUE /*just_hide*/);

	gtk_signal_connect (GTK_OBJECT (gnome_dialog), "destroy",
			    GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			    &gnome_dialog);

	label = nautilus_label_new (_("Once a day your files and text content are indexed so "
                                      "your searches are fast. If you need to update your index "
                                      "now, click on the \"Update Now\" button for the "
                                      "appropriate index."));

        nautilus_label_set_wrap (NAUTILUS_LABEL (label), TRUE);
	nautilus_label_set_justify (NAUTILUS_LABEL (label), GTK_JUSTIFY_LEFT);

	gtk_box_pack_start (GTK_BOX (gnome_dialog->vbox), label, TRUE, TRUE, 0);

        if (medusa_index_is_currently_running ()) { 
                show_index_progress_bar (gnome_dialog);
        } else {
                show_reindex_request_information (gnome_dialog);
        }

	gtk_widget_show_all (indexing_info_dialog);
}

#endif /* HAVE_MEDUSA */

static void
show_search_service_not_available_dialog (void)
{
        nautilus_error_dialog (_("Sorry, but the medusa search service is not available."),
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
