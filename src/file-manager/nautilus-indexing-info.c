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
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-label.h>
#include <libnautilus-private/nautilus-medusa-support.h>
#include <eel/eel-stock-dialogs.h>

#ifdef HAVE_MEDUSA

#include <libmedusa/medusa-index-service.h>
#include <libmedusa/medusa-index-progress.h>
#include <libmedusa/medusa-indexed-search.h>
#include <libmedusa/medusa-system-state.h>

#define PROGRESS_UPDATE_INTERVAL 5000

typedef struct {
        EelLabel *progress_label;
        GtkProgress *progress_bar;
} ProgressChangeData;

typedef struct {
        GnomeDialog *last_index_time_dialog;
        GnomeDialog *index_in_progress_dialog;
        gboolean indexing_is_in_progress;
} IndexingInfoDialogs;

static IndexingInfoDialogs *dialogs = NULL;

static GnomeDialog *   last_index_time_dialog_new      (void);

static int
get_index_percentage_complete (void)
{
        return medusa_index_progress_get_percentage_complete ();
}

/* We set up a callback so that if medusa state changes, we
   close the dialog.  To avoid race conditions with removing
   the callback entirely when we destroy it, we set it so
   that the dialog just hides, rather than closing */
static void
set_close_hides_for_dialog (GnomeDialog *dialog)
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
        EelLabel *progress_label;
        ProgressChangeData *progress_change_data;
        char *progress_string;

        progress_change_data = (ProgressChangeData *) callback_data;

        progress_label = EEL_LABEL (progress_change_data->progress_label);
        progress_string = get_text_for_progress_label ();
        eel_label_set_text (progress_label, progress_string);
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
        callback_id = medusa_execute_once_when_system_state_changes (dialog_close_cover,
                                                                     dialogs->index_in_progress_dialog);
        gtk_signal_connect_object (GTK_OBJECT (dialogs->index_in_progress_dialog),
                                   "destroy",
                                   medusa_remove_state_changed_function,
                                   GINT_TO_POINTER (callback_id));
}


static void
show_last_index_time_dialog (void)
{
        int callback_id;

        gnome_dialog_close (dialogs->index_in_progress_dialog);
        gtk_widget_show_all (GTK_WIDGET (dialogs->last_index_time_dialog));
        callback_id = medusa_execute_once_when_system_state_changes (dialog_close_cover,
                                                                     dialogs->last_index_time_dialog);
        gtk_signal_connect_object (GTK_OBJECT (dialogs->last_index_time_dialog),
                                   "destroy",
                                   medusa_remove_state_changed_function,
                                   GINT_TO_POINTER (callback_id));
}

static GnomeDialog *
last_index_time_dialog_new (void)
{
	char *time_str;
	char *label_str;
        GtkWidget *label;
        GnomeDialog *dialog;

        dialog = eel_create_info_dialog (_("Once a day your files and text content are indexed so "
                                           "your searches are fast. "),
                                         _("Indexing Status"),
                                         NULL);
        set_close_hides_for_dialog (dialog);

        time_str = nautilus_indexing_info_get_last_index_time ();
        label_str = g_strdup_printf (_("Your files were last indexed at %s"),
                                     time_str);
        g_free (time_str);
        
        label = eel_label_new (label_str);
        eel_label_set_never_smooth (EEL_LABEL (label), TRUE);
        eel_label_set_justify (EEL_LABEL (label), GTK_JUSTIFY_LEFT);
        eel_label_make_bold (EEL_LABEL (label));
        gtk_box_pack_start (GTK_BOX (dialog->vbox), label,
                            FALSE, FALSE, 0);

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
                
        dialog = eel_create_info_dialog (_("Once a day your files and text content are indexed so "
                                           "your searches are fast.  Your files are currently being indexed."),
                                         _("Indexing Status"), NULL);
        set_close_hides_for_dialog (dialog);        
        percentage_complete = get_index_percentage_complete ();
        indexing_progress_bar = gtk_progress_bar_new ();

        embedded_vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);

        gtk_progress_set_show_text (GTK_PROGRESS (indexing_progress_bar), FALSE);
        gtk_progress_configure (GTK_PROGRESS (indexing_progress_bar), percentage_complete, 0, 100);
        /* Put the progress bar in an hbox to make it a more sane size */
        gtk_box_pack_start (GTK_BOX (embedded_vbox), indexing_progress_bar, FALSE, FALSE, 0);

        progress_string = get_text_for_progress_label ();
        progress_label = eel_label_new (progress_string);
        eel_label_set_never_smooth (EEL_LABEL (progress_label), TRUE);
        g_free (progress_string);
        eel_label_set_justify (EEL_LABEL (progress_label), GTK_JUSTIFY_LEFT);
        gtk_box_pack_start (GTK_BOX (embedded_vbox), progress_label, FALSE, FALSE, 0);

        progress_bar_hbox = gtk_hbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (progress_bar_hbox), embedded_vbox,
                            FALSE, FALSE, GNOME_PAD_SMALL);

        gtk_box_pack_start (GTK_BOX (dialog->vbox), progress_bar_hbox,
                            FALSE, FALSE, 0);

        /* Keep the dialog current with actual indexing progress */
        progress_data = g_new (ProgressChangeData, 1);
        progress_data->progress_label = EEL_LABEL (progress_label);
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
        char *details_string;
        int callback_id;

        if (!medusa_system_services_are_enabled ()) {
                details_string = nautilus_medusa_get_explanation_of_enabling ();
                dialog_shown = eel_show_info_dialog_with_details (_("When Fast Search is enabled, Find creates an "
                                                                    "index to speed up searches.  Fast searching "
                                                                    "is not enabled on your computer, so you "
                                                                    "do not have an index right now."),
                                                                  _("There is no index of your files right now."),
                                                                    details_string,
                                                                  NULL);
                g_free (details_string);
                set_close_hides_for_dialog (dialog_shown);
                
                callback_id = medusa_execute_once_when_system_state_changes (dialog_close_cover,
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
                
                dialogs->last_index_time_dialog = last_index_time_dialog_new ();
                dialogs->index_in_progress_dialog = index_progress_dialog_new ();
        }
 
       dialogs->indexing_is_in_progress = medusa_indexing_is_currently_in_progress ();
       
       if (dialogs->indexing_is_in_progress) {
               show_index_progress_dialog ();
       } else {
               show_last_index_time_dialog ();
       }
        
}

#endif /* HAVE_MEDUSA */

#ifndef HAVE_MEDUSA
static void
show_search_service_not_available_dialog (void)
{
        eel_show_error_dialog (_("Sorry, but the medusa search service is not available."),
                                    _("Search Service Not Available"),
                                    NULL);
}
#endif


char *
nautilus_indexing_info_get_last_index_time (void)
{
#ifdef HAVE_MEDUSA
	time_t update_time;
        
	update_time = medusa_index_service_get_last_index_update_time ();
        if (update_time) {
                return eel_strdup_strftime (_("%I:%M %p, %x"),
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
        show_indexing_info_dialog ();
#else
        show_search_service_not_available_dialog ();
#endif
}
