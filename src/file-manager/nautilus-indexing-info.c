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
#include <gnome.h>

#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libgnomeui/gnome-uidefs.h>

/* FOR THE INCREDIBLE HACK */
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>


#include <libmedusa/medusa-index-service.h>
#include <libmedusa/medusa-index-progress.h>
#include <libmedusa/medusa-indexed-search.h>

#include "nautilus-indexing-info.h"

typedef struct {
        NautilusLabel *progress_label;
        GtkProgress *progress_bar;
} ProgressChangeData;

static GtkWidget *dialog = NULL;

static char *
get_text_for_progress_label (void)
{
        return g_strdup_printf ("Indexing is %d%% complete.", medusa_index_progress_get_percentage_complete ());
}

static gint
update_progress_display (gpointer data)
{
        NautilusLabel *progress_label;
        ProgressChangeData *progress_change_data;
        char *progress_string;

        g_return_val_if_fail (data != NULL, 0);
        progress_change_data = (ProgressChangeData *) data;
        if (dialog == NULL) {
                return 0;
        }
        
        /* This shouldn't ever happen, but it is possible, so we'll
         * not whine if it does.
         */
        if (!NAUTILUS_IS_LABEL (progress_change_data->progress_label)) {
                return 0;
        }
                                                                              
        g_return_val_if_fail (GTK_IS_PROGRESS (progress_change_data->progress_bar), 0);
        progress_label = NAUTILUS_LABEL (progress_change_data->progress_label);
        progress_string = get_text_for_progress_label ();
        nautilus_label_set_text (progress_label,
                                 progress_string);
        g_free (progress_string);
        gtk_progress_set_value (progress_change_data->progress_bar,
                                medusa_index_progress_get_percentage_complete ());

        return 1;
}


static void
update_file_index_callback (GtkWidget *widget, gpointer data)
{
	char *error = NULL;
	MedusaIndexingRequestResult result;

	result = medusa_index_service_request_reindex ();

	switch (result) {
	default:
	case MEDUSA_INDEXER_REQUEST_OK:
		error = NULL;
		break;
	case MEDUSA_INDEXER_ERROR_BUSY:
		error = _("Busy");
		break;
	case MEDUSA_INDEXER_ERROR_NO_RESPONSE:
		error = _("No response");
		break;
	case MEDUSA_INDEXER_ERROR_NO_INDEXER_PRESENT:
		error = _("No indexer present");
		break;
	}

	if (error != NULL) {
		char *string;
		GtkWidget *error_dialog;

		string = g_strdup_printf (_("Error while trying "
					    "to reindex: %s"),
					  error);
		error_dialog = gnome_error_dialog (string);
		gnome_dialog_run (GNOME_DIALOG (error_dialog));
		g_free(string);
	}
}

static void
make_label_helvetica_bold (NautilusLabel *label)
{
        NautilusScalableFont *scalable_font;

        scalable_font = NAUTILUS_SCALABLE_FONT (nautilus_scalable_font_new ("helvetica", "bold", NULL, NULL));
        g_return_if_fail (scalable_font != NULL);
        nautilus_label_set_font (label,
                                 scalable_font);
        gtk_object_unref (GTK_OBJECT (scalable_font));
       
}

static void
make_label_helvetica_medium (NautilusLabel *label)
{
        NautilusScalableFont *scalable_font;

        scalable_font = NAUTILUS_SCALABLE_FONT (nautilus_scalable_font_new ("helvetica", "medium", NULL, NULL));
        g_return_if_fail (scalable_font != NULL);
        nautilus_label_set_font (NAUTILUS_LABEL (label),
                                 scalable_font);
}
static char *
get_file_index_time (void)
{
        struct tm *time_struct;
	time_t the_time;
        char *time_string;

	the_time = medusa_index_service_get_last_index_update_time ();
        time_struct = localtime (&the_time);

        
        time_string = nautilus_strdup_strftime (_("%I:%M %p, %x"), time_struct);

        return time_string;
}

static void 
show_reindex_request_information (GnomeDialog *gnome_dialog)
{
	char *time_str;
	char *label_str;
        GtkWidget *label;
        GtkWidget *button;
        GtkWidget *hbox;


        time_str = get_file_index_time ();
        label_str = g_strdup_printf (_("Your files were last indexed at %s"),
                                     time_str);
        g_free (time_str);

        label = nautilus_label_new (label_str);
        nautilus_label_set_text_justification (NAUTILUS_LABEL (label), GTK_JUSTIFY_LEFT);
        make_label_helvetica_bold (NAUTILUS_LABEL (label));
        gtk_box_pack_start (GTK_BOX (gnome_dialog->vbox), label,
                            FALSE, FALSE, 0);
        
        hbox = gtk_hbox_new (FALSE, 0);
        button = gtk_button_new_with_label (_("Update Now"));
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                            GTK_SIGNAL_FUNC (update_file_index_callback),
                            NULL);

        gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (gnome_dialog->vbox), hbox,
                            FALSE, FALSE, 0);
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
                
        indexing_notification_label = nautilus_label_new (_("Your files are currently being indexed:"));
        make_label_helvetica_bold (NAUTILUS_LABEL (indexing_notification_label));
        nautilus_label_set_text_justification (NAUTILUS_LABEL (indexing_notification_label), GTK_JUSTIFY_LEFT);
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
        make_label_helvetica_medium (NAUTILUS_LABEL (progress_label));
        nautilus_label_set_text_justification (NAUTILUS_LABEL (progress_label), GTK_JUSTIFY_LEFT);
        gtk_box_pack_start (GTK_BOX (embedded_vbox), progress_label, FALSE, FALSE, 0);

        progress_bar_hbox = gtk_hbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (progress_bar_hbox), embedded_vbox,
                            FALSE, FALSE, GNOME_PAD_SMALL);

        gtk_box_pack_start (GTK_BOX (gnome_dialog->vbox), progress_bar_hbox,
                                    FALSE, FALSE, 0);

        /* Keep the dialog current with actual indexing progress */
        progress_data = g_new0 (ProgressChangeData, 1);
        progress_data->progress_label = NAUTILUS_LABEL (progress_label);
        progress_data->progress_bar = GTK_PROGRESS (indexing_progress_bar);
        gtk_timeout_add_full (5000,
                              update_progress_display,
                              NULL,
                              progress_data,
                              g_free);
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

	GnomeDialog *gnome_dialog;
        GtkWidget *label;

        /* FIXME bugzilla.eazel.com 2534:  is it ok to show the index dialog if
           we can't use the index right now?
           This assumes not */
	if (medusa_indexed_search_is_available () != GNOME_VFS_OK) {
		GtkWidget *error =
			gnome_error_dialog (_("Search service not available"));
		gnome_dialog_run (GNOME_DIALOG (error));
		return;
	}

	/* A dialog is up already */
	if (dialog != NULL) {
		gtk_widget_show_now (dialog);
		gdk_window_raise (dialog->window);
		return;
	}

	dialog = gnome_dialog_new (_("Indexing Info..."),
				   GNOME_STOCK_BUTTON_OK,
				   NULL);

	gnome_dialog = GNOME_DIALOG (dialog);

	gnome_dialog_set_close (gnome_dialog, TRUE /*click_closes*/);
	gnome_dialog_close_hides (gnome_dialog, FALSE /*just_hide*/);

	gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
			    GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			    &dialog);

	label = nautilus_label_new (_("Once a day your files and text content are indexed so\n"
                                      "your searches are fast.  If you need to update your index\n"
                                      "now, click on the \"Update Now\" button for the\n"
                                      "appropriate index.\n"));
        make_label_helvetica_medium (NAUTILUS_LABEL (label));

	nautilus_label_set_text_justification (NAUTILUS_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start (GTK_BOX (gnome_dialog->vbox), label, TRUE, TRUE, 0);

        if (medusa_index_is_currently_running ()) { 
                show_index_progress_bar (gnome_dialog);

        } else {
                show_reindex_request_information (gnome_dialog);
        }

	gtk_widget_show_all (dialog);
}
