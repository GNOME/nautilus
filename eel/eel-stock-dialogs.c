/* eel-stock-dialogs.c: Various standard dialogs for Eel.
 *
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  The Gnome Library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Gnome Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with the Gnome Library; see the file COPYING.LIB.  If not,
 *  see <http://www.gnu.org/licenses/>.
 *
 *  Authors: Darin Adler <darin@eazel.com>
 */

#include <config.h>
#include "eel-stock-dialogs.h"

#include "eel-glib-extensions.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <libadwaita-1/adwaita.h>

#define TIMED_WAIT_STANDARD_DURATION 2000
#define TIMED_WAIT_MIN_TIME_UP 3000

#define TIMED_WAIT_MINIMUM_DIALOG_WIDTH 300

#define RESPONSE_DETAILS 1000

typedef struct
{
    EelCancelCallback cancel_callback;
    gpointer callback_data;

    /* Parameters for creation of the window. */
    char *wait_message;
    GtkWindow *parent_window;

    /* Timer to determine when we need to create the window. */
    guint timeout_handler_id;

    /* Window, once it's created. */
    GtkWidget *dialog;

    /* system time (microseconds) when dialog was created */
    gint64 dialog_creation_time;
} TimedWait;

static GHashTable *timed_wait_hash_table;

static void timed_wait_dialog_destroy_callback (GtkWidget *object,
                                                gpointer   callback_data);

static guint
timed_wait_hash (gconstpointer value)
{
    const TimedWait *wait;

    wait = value;

    return GPOINTER_TO_UINT (wait->cancel_callback)
           ^ GPOINTER_TO_UINT (wait->callback_data);
}

static gboolean
timed_wait_hash_equal (gconstpointer value1,
                       gconstpointer value2)
{
    const TimedWait *wait1, *wait2;

    wait1 = value1;
    wait2 = value2;

    return wait1->cancel_callback == wait2->cancel_callback
           && wait1->callback_data == wait2->callback_data;
}

static void
timed_wait_delayed_close_destroy_dialog_callback (GtkWidget *object,
                                                  gpointer   callback_data)
{
    g_source_remove (GPOINTER_TO_UINT (callback_data));
}

static gboolean
timed_wait_delayed_close_timeout_callback (gpointer callback_data)
{
    guint handler_id;

    handler_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (callback_data),
                                                      "eel-stock-dialogs/delayed_close_handler_timeout_id"));

    g_signal_handlers_disconnect_by_func (G_OBJECT (callback_data),
                                          G_CALLBACK (timed_wait_delayed_close_destroy_dialog_callback),
                                          GUINT_TO_POINTER (handler_id));

    gtk_window_destroy (GTK_WINDOW (callback_data));

    return FALSE;
}

static void
timed_wait_free (TimedWait *wait)
{
    guint delayed_close_handler_id;
    guint64 time_up;

    g_assert (g_hash_table_lookup (timed_wait_hash_table, wait) != NULL);

    g_hash_table_remove (timed_wait_hash_table, wait);

    g_free (wait->wait_message);
    if (wait->parent_window != NULL)
    {
        g_object_unref (wait->parent_window);
    }
    if (wait->timeout_handler_id != 0)
    {
        g_source_remove (wait->timeout_handler_id);
    }
    if (wait->dialog != NULL)
    {
        /* Make sure to detach from the "destroy" signal, or we'll
         * double-free.
         */
        g_signal_handlers_disconnect_by_func (G_OBJECT (wait->dialog),
                                              G_CALLBACK (timed_wait_dialog_destroy_callback),
                                              wait);

        /* compute time up in milliseconds */
        time_up = (g_get_monotonic_time () - wait->dialog_creation_time) / 1000;

        if (time_up < TIMED_WAIT_MIN_TIME_UP)
        {
            delayed_close_handler_id = g_timeout_add (TIMED_WAIT_MIN_TIME_UP - time_up,
                                                      timed_wait_delayed_close_timeout_callback,
                                                      wait->dialog);
            g_object_set_data (G_OBJECT (wait->dialog),
                               "eel-stock-dialogs/delayed_close_handler_timeout_id",
                               GUINT_TO_POINTER (delayed_close_handler_id));
            g_signal_connect (wait->dialog, "destroy",
                              G_CALLBACK (timed_wait_delayed_close_destroy_dialog_callback),
                              GUINT_TO_POINTER (delayed_close_handler_id));
        }
        else
        {
            gtk_window_destroy (GTK_WINDOW (wait->dialog));
        }
    }

    /* And the wait object itself. */
    g_free (wait);
}

static void
timed_wait_dialog_destroy_callback (GtkWidget *object,
                                    gpointer   callback_data)
{
    TimedWait *wait;

    wait = callback_data;

    g_assert (object == wait->dialog);

    wait->dialog = NULL;

    /* When there's no cancel_callback, the originator will/must
     * call eel_timed_wait_stop which will call timed_wait_free.
     */

    if (wait->cancel_callback != NULL)
    {
        (*wait->cancel_callback)(wait->callback_data);
        timed_wait_free (wait);
    }
}

static gboolean
timed_wait_callback (gpointer callback_data)
{
    TimedWait *wait;
    GtkWidget *dialog;

    wait = callback_data;

    /* Put up the timed wait window. */
    dialog = adw_message_dialog_new (wait->parent_window,
                                     wait->wait_message,
                                     _("You can stop this operation by clicking cancel."));

    adw_message_dialog_add_response (ADW_MESSAGE_DIALOG (dialog), "cancel", _("_Cancel"));
    adw_message_dialog_set_default_response (ADW_MESSAGE_DIALOG (dialog), "cancel");

    wait->dialog_creation_time = g_get_monotonic_time ();
    gtk_window_present (GTK_WINDOW (dialog));

    /* FIXME bugzilla.eazel.com 2441:
     * Could parent here, but it's complicated because we
     * don't want this window to go away just because the parent
     * would go away first.
     */

    /* Make the dialog cancel the timed wait when it goes away.
     * Connect to "destroy" instead of "response" since we want
     * to be called no matter how the dialog goes away.
     */
    g_signal_connect (dialog, "destroy",
                      G_CALLBACK (timed_wait_dialog_destroy_callback),
                      wait);

    wait->timeout_handler_id = 0;
    wait->dialog = dialog;

    return FALSE;
}

void
eel_timed_wait_start_with_duration (int                duration,
                                    EelCancelCallback  cancel_callback,
                                    gpointer           callback_data,
                                    const char        *wait_message,
                                    GtkWindow         *parent_window)
{
    TimedWait *wait;

    g_return_if_fail (cancel_callback != NULL);
    g_return_if_fail (callback_data != NULL);
    g_return_if_fail (wait_message != NULL);
    g_return_if_fail (parent_window == NULL || GTK_IS_WINDOW (parent_window));

    /* Create the timed wait record. */
    wait = g_new0 (TimedWait, 1);
    wait->wait_message = g_strdup (wait_message);
    wait->cancel_callback = cancel_callback;
    wait->callback_data = callback_data;
    wait->parent_window = parent_window;

    if (parent_window != NULL)
    {
        g_object_ref (parent_window);
    }

    /* Start the timer. */
    wait->timeout_handler_id = g_timeout_add (duration, timed_wait_callback, wait);

    /* Put in the hash table so we can find it later. */
    if (timed_wait_hash_table == NULL)
    {
        timed_wait_hash_table = g_hash_table_new (timed_wait_hash, timed_wait_hash_equal);
    }
    g_assert (g_hash_table_lookup (timed_wait_hash_table, wait) == NULL);
    g_hash_table_insert (timed_wait_hash_table, wait, wait);
    g_assert (g_hash_table_lookup (timed_wait_hash_table, wait) == wait);
}

void
eel_timed_wait_start (EelCancelCallback  cancel_callback,
                      gpointer           callback_data,
                      const char        *wait_message,
                      GtkWindow         *parent_window)
{
    eel_timed_wait_start_with_duration
        (TIMED_WAIT_STANDARD_DURATION,
        cancel_callback, callback_data,
        wait_message, parent_window);
}

void
eel_timed_wait_stop (EelCancelCallback cancel_callback,
                     gpointer          callback_data)
{
    TimedWait key;
    TimedWait *wait;

    g_return_if_fail (callback_data != NULL);

    key.cancel_callback = cancel_callback;
    key.callback_data = callback_data;
    wait = g_hash_table_lookup (timed_wait_hash_table, &key);

    g_return_if_fail (wait != NULL);

    timed_wait_free (wait);
}
