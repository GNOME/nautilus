/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-clipboard.c
 *
 * Nautilus Clipboard support.  For now, routines to support component cut
 * and paste.
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 * Copyright (C) 2000  Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Rebecca Schulman <rebecka@eazel.com>,
 *          Darin Adler <darin@eazel.com>
 */

#include <config.h>
#include "nautilus-clipboard.h"

#include "nautilus-bonobo-ui.h"
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

#ifdef UIH
static void
cut_callback (BonoboUIHandler *ui_handler,
	      gpointer callback_data,
	      const char *path)
{
	g_assert (BONOBO_IS_UI_HANDLER (ui_handler));
	g_assert (strcmp (path, NAUTILUS_MENU_PATH_CUT_ITEM) == 0);
	
	gtk_editable_cut_clipboard (GTK_EDITABLE (callback_data));
}

static void
copy_callback (BonoboUIHandler *ui_handler,
	       gpointer callback_data,
	       const char *path)
{
	g_assert (BONOBO_IS_UI_HANDLER (ui_handler));
	g_assert (strcmp (path, NAUTILUS_MENU_PATH_COPY_ITEM) == 0);
	
	gtk_editable_copy_clipboard (GTK_EDITABLE (callback_data));
}

static void
paste_callback (BonoboUIHandler *ui_handler,
		gpointer callback_data,
		const char *path)
{
	g_assert (BONOBO_IS_UI_HANDLER (ui_handler));
	g_assert (strcmp (path, NAUTILUS_MENU_PATH_PASTE_ITEM) == 0);
	
	gtk_editable_paste_clipboard (GTK_EDITABLE (callback_data));
}

static void
clear_callback (BonoboUIHandler *ui_handler,
		gpointer callback_data,
		const char *path)
{
	g_assert (BONOBO_IS_UI_HANDLER (ui_handler));
	g_assert (strcmp (path, NAUTILUS_MENU_PATH_CLEAR_ITEM) == 0);
	
	/* A negative index deletes until the end of the string */
	gtk_editable_delete_text (GTK_EDITABLE (callback_data), 0, -1);
}

static void
add_menu_item (BonoboUIHandler *ui_handler,
	       const char *path,
	       const char *title,
	       const char *description,
	       BonoboUIHandlerCallback callback,
	       gpointer callback_data)
{
	bonobo_ui_handler_menu_new_item
		(ui_handler, path, title, description,
		 bonobo_ui_handler_menu_get_pos (ui_handler, path),
		 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL, 0, 0,
		 callback, callback_data);
}
#endif /* UIH */

/*
static void
set_paste_sensitive_if_clipboard_contains_data (BonoboUIHandler *ui_handler)
{
	gboolean clipboard_contains_data;
	
	clipboard_contains_data = 
		(gdk_selection_owner_get (GDK_SELECTION_PRIMARY) != NULL);

	bonobo_ui_handler_menu_set_sensitivity (ui_handler,
						NAUTILUS_MENU_PATH_PASTE_ITEM,
						clipboard_contains_data);
}
*/
static void
add_menu_items_callback (GtkWidget *widget,
			 GdkEventAny *event,
			 gpointer callback_data)
{
#ifdef UIH

        BonoboUIHandler *local_ui_handler;
	Bonobo_UIHandler remote_ui_handler;

	g_assert (GTK_IS_EDITABLE (widget));
	
	local_ui_handler = bonobo_control_get_ui_handler (BONOBO_CONTROL (callback_data));

	remote_ui_handler = bonobo_control_get_remote_ui_handler (BONOBO_CONTROL (callback_data));
	bonobo_ui_handler_set_container (local_ui_handler, remote_ui_handler);
	bonobo_object_release_unref (remote_ui_handler, NULL);

	add_menu_item (local_ui_handler,
		       NAUTILUS_MENU_PATH_CUT_ITEM,
		       _("_Cut Text"),
		       _("Cuts the selected text to the clipboard"),
		       cut_callback, widget);
	add_menu_item (local_ui_handler,
		       NAUTILUS_MENU_PATH_COPY_ITEM,
		       _("_Copy Text"),
		       _("Copies the selected text to the clipboard"),
		       copy_callback, widget);
	add_menu_item (local_ui_handler,
		       NAUTILUS_MENU_PATH_PASTE_ITEM,
		       _("_Paste Text"),
		       _("Pastes the text stored on the clipboard"),
		       paste_callback, widget);
	add_menu_item (local_ui_handler,
		       NAUTILUS_MENU_PATH_CLEAR_ITEM,
		       _("C_lear Text"),
		       _("Removes the selected text without putting it on the clipboard"),
		       clear_callback, widget);
#endif
}

static void
remove_menu_items_callback (GtkWidget *widget,
			    GdkEventAny *event,
			    gpointer callback_data)
{
#ifdef UIH
	BonoboUIHandler *ui_handler;

	g_assert (GTK_IS_EDITABLE (widget));

	ui_handler = bonobo_control_get_ui_handler (BONOBO_CONTROL (callback_data)); 

	bonobo_ui_handler_menu_remove (ui_handler,
				       NAUTILUS_MENU_PATH_CUT_ITEM);
	bonobo_ui_handler_menu_remove (ui_handler,
				       NAUTILUS_MENU_PATH_COPY_ITEM);
	bonobo_ui_handler_menu_remove (ui_handler,
				       NAUTILUS_MENU_PATH_PASTE_ITEM);
	bonobo_ui_handler_menu_remove (ui_handler,
				       NAUTILUS_MENU_PATH_CLEAR_ITEM);
#endif
}

/*
static void
set_clipboard_menu_items_sensitive (BonoboUIHandler *ui_handler)
{

       	bonobo_ui_handler_menu_set_sensitivity (ui_handler,
						NAUTILUS_MENU_PATH_CUT_ITEM,
						TRUE);
	bonobo_ui_handler_menu_set_sensitivity (ui_handler,
						NAUTILUS_MENU_PATH_COPY_ITEM,
						TRUE);
	bonobo_ui_handler_menu_set_sensitivity (ui_handler,
						NAUTILUS_MENU_PATH_CLEAR_ITEM,
						TRUE);
	set_paste_sensitive_if_clipboard_contains_data (ui_handler);
}


static void
set_clipboard_menu_items_insensitive (BonoboUIHandler *ui_handler,
				      gboolean enable_paste_for_full_clipboard)
{
	
       	bonobo_ui_handler_menu_set_sensitivity (ui_handler,
						NAUTILUS_MENU_PATH_CUT_ITEM,
						FALSE);
	bonobo_ui_handler_menu_set_sensitivity (ui_handler,
						NAUTILUS_MENU_PATH_COPY_ITEM,
						FALSE);
	bonobo_ui_handler_menu_set_sensitivity (ui_handler,
						NAUTILUS_MENU_PATH_CLEAR_ITEM,
						FALSE);
	if (enable_paste_for_full_clipboard) {
		set_paste_sensitive_if_clipboard_contains_data (ui_handler);
	}
	else {
		bonobo_ui_handler_menu_set_sensitivity (ui_handler,
							NAUTILUS_MENU_PATH_PASTE_ITEM,
							FALSE);
	}
}
*/

void
nautilus_clipboard_set_up_editable_from_bonobo_control (GtkEditable *target,
							BonoboControl *control)
{
	g_return_if_fail (GTK_IS_EDITABLE (target));
	g_return_if_fail (BONOBO_IS_CONTROL (control));

	/* Attach code to add menus when it gets the focus. */
	gtk_signal_connect_while_alive
		(GTK_OBJECT (target), "focus_in_event",
		 GTK_SIGNAL_FUNC (add_menu_items_callback),
		 control, GTK_OBJECT (control));
	/* Attach code to remove menus when it loses the focus. */
 	gtk_signal_connect_while_alive
		(GTK_OBJECT (target), "focus_out_event",
		 GTK_SIGNAL_FUNC (remove_menu_items_callback),
		 control, GTK_OBJECT (control));

}

