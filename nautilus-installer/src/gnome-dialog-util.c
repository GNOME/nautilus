/* GNOME GUI Library: gnome-dialog-util.c
 * Copyright (C) 1998 Free Software Foundation
 * Author: Havoc Pennington
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Cambridge, MA 02139, USA.
 */
#include <config.h>
#include "gnome-messagebox.h"
#include "gnome-types.h"
#include "gnome-dialog-util.h"
#include "fake-stock.h"
#include <gtk/gtk.h>


typedef struct {
  gpointer function;
  gpointer data;
  GtkEntry * entry;
} callback_info;

static void
dialog_reply_callback (GnomeMessageBox * mbox, gint button, callback_info* data)
{
  GnomeReplyCallback func = (GnomeReplyCallback) data->function;
  (* func)(button, data->data);
}
 
static GtkWidget *
reply_dialog (const gchar * question, GnomeReplyCallback callback, gpointer data,
	      gboolean yes_or_ok, gboolean modal, GtkWindow * parent)
{
  GtkWidget * mbox;
  callback_info * info;

  mbox = gnome_message_box_new(question, GNOME_MESSAGE_BOX_QUESTION,
			       GNOME_STOCK_BUTTON_YES, 
			       GNOME_STOCK_BUTTON_NO, NULL);

  if (callback != NULL) {
    info = g_new(callback_info, 1);

    info->function = callback;
    info->data = data;

    gtk_signal_connect_full(GTK_OBJECT(mbox), "clicked",
			    GTK_SIGNAL_FUNC(dialog_reply_callback),
			    NULL,
			    info,
			    (GtkDestroyNotify)g_free,
			    FALSE, FALSE);
  }

  if (parent != NULL) {
    gnome_dialog_set_parent(GNOME_DIALOG(mbox),parent);
  }

  gtk_widget_show(mbox);
  return mbox;
}


/* Ask a yes or no question, and call the callback when it's answered. */

GtkWidget *
gnome_question_dialog (const gchar * question,
		       GnomeReplyCallback callback, gpointer data)
{
  return reply_dialog(question, callback, data, TRUE, FALSE, NULL);
}

GtkWidget *
gnome_question_dialog_parented (const gchar * question,
				GnomeReplyCallback callback, gpointer data,
				GtkWindow * parent)
{
  return reply_dialog(question, callback, data, TRUE, FALSE, parent);
}

