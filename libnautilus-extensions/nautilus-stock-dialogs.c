/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-stock-dialogs.c: Various standard dialogs for Nautilus.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "nautilus-stock-dialogs.h"

struct NautilusTimedWait {
	char *window_title;
	char *wait_message;
	NautilusCancelCallback cancel_callback;
	gpointer callback_data;
	GDestroyNotify destroy_notify;
	GtkWindow *parent_window;
};

NautilusTimedWait *
nautilus_timed_wait_start (const char *window_title,
			   const char *wait_message,
			   NautilusCancelCallback cancel_callback,
			   gpointer callback_data,
			   GDestroyNotify destroy_notify,
			   GtkWindow *parent_window)
{
	NautilusTimedWait *timed_wait;

	g_return_val_if_fail (window_title != NULL, NULL);
	g_return_val_if_fail (wait_message != NULL, NULL);
	g_return_val_if_fail (cancel_callback != NULL, NULL);
	g_return_val_if_fail (parent_window == NULL || GTK_IS_WINDOW (parent_window), NULL);

	/* Create the timed wait record. */
	timed_wait = g_new (NautilusTimedWait, 1);
	timed_wait->window_title = g_strdup (window_title);
	timed_wait->wait_message = g_strdup (wait_message);
	timed_wait->cancel_callback = cancel_callback;
	timed_wait->callback_data = callback_data;
	timed_wait->destroy_notify = destroy_notify;
	timed_wait->parent_window = parent_window;
	if (parent_window != NULL) {
		gtk_widget_ref (GTK_WIDGET (parent_window));
	}

	return timed_wait;
}

static void
nautilus_timed_wait_free (NautilusTimedWait *timed_wait)
{
	/* Let the caller destroy the callback data. */
	if (timed_wait->destroy_notify != NULL) {
		(* timed_wait->destroy_notify) (timed_wait->callback_data);
	}

	/* Now free the other stuff we were holding onto. */
	g_free (timed_wait->window_title);
	g_free (timed_wait->wait_message);
	if (timed_wait->parent_window != NULL) {
		gtk_widget_unref (GTK_WIDGET (timed_wait->parent_window));
	}

	/* And the wait object itself. */
	g_free (timed_wait);
}

void
nautilus_timed_wait_stop (NautilusTimedWait *timed_wait)
{
	g_return_if_fail (timed_wait != NULL);

	nautilus_timed_wait_free (timed_wait);
}
