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

/* FOR THE INCREDIBLE HACK */
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>


#include <libmedusa/medusa-index-service.h>
#include <libmedusa/medusa-indexed-search.h>

#include "nautilus-indexing-info.h"

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
		GtkWidget *dialog;

		string = g_strdup_printf (_("Error while trying "
					    "to reindex: %s"),
					  error);
		dialog = gnome_error_dialog (string);
		gnome_dialog_run (GNOME_DIALOG (dialog));
		g_free(string);
	}
}

static void
make_label_bold (GtkWidget *label)
{
	GdkFont *bold_font;

	bold_font = nautilus_gdk_font_get_bold (label->style->font);

	if (bold_font != NULL) {
		nautilus_gtk_widget_set_font (label, bold_font);
	}
}

static char *
get_file_index_time (void)
{
	time_t the_time;
	char *time_str, *p;
	
	the_time = medusa_index_service_get_last_index_update_time ();

	time_str = g_strdup (ctime (&the_time));

	p = strchr (time_str, '\n');
	if (p) {
		*p = '\0';
	}

	return time_str;
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
	GtkWidget *label, *button;
	GtkWidget *hbox;
	static GtkWidget *dialog = NULL;
	GnomeDialog *gnome_dialog;
	char *time_str;
	char *label_str;

        /* FIXME:  is it ok to show the index dialog if
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

	label = gtk_label_new (_("Once a day your files and text content are "
				 "indexed so your searches are fast.  If you "
				 "need to update your index now, click on the "
				 "\"Update Now\" button for the appropriate "
				 "index."));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (gnome_dialog->vbox), label, TRUE, TRUE, 0);

	time_str = get_file_index_time ();
	label_str = g_strdup_printf (_("Your files were last indexed at  %s"),
				     time_str);
	g_free (time_str);
	label = gtk_label_new (label_str);
	make_label_bold (label);
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

	gtk_widget_show_all (dialog);
}
