/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* nautilus-clipboard.c
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
 * Author: Rebecca Schulman <rebecka@eazel.com>
 */
#include <config.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtkscrolledwindow.h>
#include <gnome.h>
#include <bonobo/bonobo-control.h>

#include "nautilus-clipboard.h"


#define MENU_PATH_CUT			     "/Edit/Cut"
#define MENU_PATH_COPY			     "/Edit/Copy"
#define MENU_PATH_PASTE			     "/Edit/Paste"
#define MENU_PATH_CLEAR			     "/Edit/Clear"


#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>


/* forward declarations */

static void             nautilus_clipboard_info_initialize_class                     (NautilusClipboardInfoClass *klass);
void                    nautilus_clipboard_info_initialize                           (NautilusClipboardInfo *info);
void                    nautilus_clipboard_info_destroy                              (NautilusClipboardInfo *info);		

void                    nautilus_clipboard_info_destroy_cb                           (GtkObject *object, gpointer user_data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusClipboardInfo, nautilus_clipboard_info, GTK_TYPE_SCROLLED_WINDOW)

NautilusClipboardInfo*  nautilus_clipboard_info_new                                  (void);

static BonoboControl *  nautilus_clipboard_info_get_bonobo_control                   (NautilusClipboardInfo *info);
/* static BonoboUIHandler* nautilus_clipboard_info_get_bonobo_ui_handler (NautilusClipboardInfo *info); */
static void             nautilus_component_bonobo_menu_cut_cb                        (BonoboUIHandler *uihandler,
										    gpointer data, const char *path);
static void             nautilus_component_bonobo_menu_copy_cb                       (BonoboUIHandler *uihandler,
										    gpointer data, const char *path);
static void             nautilus_component_bonobo_menu_paste_cb                      (BonoboUIHandler *uihandler,
										    gpointer data, const char *path);
static void             nautilus_component_bonobo_menu_clear_cb                      (BonoboUIHandler *uihandler,
										     gpointer data, const char *path);

struct NautilusClipboardDetails {
	char *component_name;
	GtkWidget *clipboard_owner;
	NautilusView *view;
};



NautilusClipboardInfo*
nautilus_clipboard_info_new ()
{
	return NAUTILUS_CLIPBOARD_INFO (gtk_type_new (nautilus_clipboard_info_get_type ()));
}



void
nautilus_clipboard_info_initialize (NautilusClipboardInfo *info)
{

	info->details = g_new0 (NautilusClipboardDetails, 1);
	info->details->component_name = NULL;
	info->details->clipboard_owner = NULL;
	info->details->view = NULL;
}





static void
nautilus_clipboard_info_initialize_class (NautilusClipboardInfoClass *klass)
{
	
	klass->destroy = nautilus_clipboard_info_destroy;

}


void
nautilus_clipboard_info_destroy (NautilusClipboardInfo *info)
{

	g_free (info->details->component_name);
	gtk_widget_unref (info->details->clipboard_owner);

	if (info->details->view != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (info->details->view));
	}

	g_free (info->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (GTK_OBJECT (info))); 
}

void
nautilus_clipboard_info_destroy_cb (GtkObject *object, gpointer user_data)
{
	NautilusClipboardInfo *view;
	
	g_assert (NAUTILUS_IS_CLIPBOARD_INFO (user_data));
	view = NAUTILUS_CLIPBOARD_INFO (user_data);

	nautilus_clipboard_info_destroy (view);

}


void
nautilus_clipboard_info_set_component_name (NautilusClipboardInfo *info, char *component_name)
{
	info->details->component_name = g_strdup(component_name);
}

char*
nautilus_clipboard_info_get_component_name (NautilusClipboardInfo *info)
{

	return info->details->component_name;
}



void
nautilus_clipboard_info_set_clipboard_owner (NautilusClipboardInfo *info, GtkWidget *clipboard_owner)
{
	g_assert (GTK_IS_EDITABLE (clipboard_owner));
	info->details->clipboard_owner = clipboard_owner;
	gtk_widget_ref (clipboard_owner);
}

GtkWidget *
nautilus_clipboard_info_get_clipboard_owner (NautilusClipboardInfo *info)
{
	return info->details->clipboard_owner;
}

void
nautilus_clipboard_info_set_view (NautilusClipboardInfo *info, NautilusView *view)
{
	g_return_if_fail (info != NULL);
	g_return_if_fail (NAUTILUS_CLIPBOARD_INFO (info));
	g_return_if_fail (view != NULL);
	g_return_if_fail (NAUTILUS_IS_VIEW (view));

	bonobo_object_ref (BONOBO_OBJECT (view));
	
	info->details->view = view;

}

NautilusView *
nautilus_clipboard_info_get_view (NautilusClipboardInfo *info)
{
	return info->details->view;
}


/*
static BonoboUIHandler *
nautilus_clipboard_info_get_bonobo_ui_handler (NautilusClipboardInfo *info)
{
        return bonobo_control_get_ui_handler (nautilus_clipboard_info_get_bonobo_control (info));
}
*/

static BonoboControl *
nautilus_clipboard_info_get_bonobo_control (NautilusClipboardInfo *info)
{
        return nautilus_view_get_bonobo_control (info->details->view);
}


void
nautilus_component_merge_bonobo_items_cb (GtkWidget *widget, GdkEventAny *event, gpointer user_data)
{
	NautilusClipboardInfo *info;
	BonoboUIHandler *local_ui_handler;


	g_assert (NAUTILUS_IS_CLIPBOARD_INFO(user_data));
	info = NAUTILUS_CLIPBOARD_INFO(user_data);
	g_assert (BONOBO_IS_CONTROL (nautilus_clipboard_info_get_bonobo_control (info)));

	local_ui_handler = bonobo_control_get_ui_handler (nautilus_clipboard_info_get_bonobo_control (info));

	bonobo_ui_handler_set_container (local_ui_handler,
					 bonobo_control_get_remote_ui_handler (nautilus_clipboard_info_get_bonobo_control (info)));
	
	bonobo_ui_handler_menu_new_item (local_ui_handler,
						 MENU_PATH_CUT,
					 _("_Cut"),
					 _("Remove selected text from selection"),
					 bonobo_ui_handler_menu_get_pos (local_ui_handler , MENU_PATH_CUT),
					 BONOBO_UI_HANDLER_PIXMAP_NONE,
					 NULL,
					 0,
					 0,
					 nautilus_component_bonobo_menu_cut_cb,
					 info);
	bonobo_ui_handler_menu_new_item (local_ui_handler,
					 MENU_PATH_COPY,
					 _("_Copy"),
					 _("Copy selected text to the clipboard"),
					 bonobo_ui_handler_menu_get_pos (local_ui_handler , MENU_PATH_COPY),
					 BONOBO_UI_HANDLER_PIXMAP_NONE,
					 NULL,
					 0,
					 0,
					 nautilus_component_bonobo_menu_copy_cb,
					 info);
	bonobo_ui_handler_menu_new_item (local_ui_handler,
					 MENU_PATH_PASTE,
					 _("_Paste"),
					 _("Paste text from clipboard into text box"),
					 bonobo_ui_handler_menu_get_pos (local_ui_handler , MENU_PATH_PASTE),
						 BONOBO_UI_HANDLER_PIXMAP_NONE,
					 NULL,
					 0,
					 0,
						 nautilus_component_bonobo_menu_paste_cb,
					 info);
	bonobo_ui_handler_menu_new_item (local_ui_handler,
					 MENU_PATH_CLEAR,
					 _("_Clear"),
					 _("Clear the current selection"),
					 bonobo_ui_handler_menu_get_pos (local_ui_handler , MENU_PATH_CLEAR),
					 BONOBO_UI_HANDLER_PIXMAP_NONE,
					 NULL,
					 0,
					 0,
					 nautilus_component_bonobo_menu_clear_cb,
					 info);
		

}


void
nautilus_component_unmerge_bonobo_items_cb (GtkWidget *widget, GdkEventAny *event, gpointer user_data)
{
	NautilusClipboardInfo *info;
	BonoboUIHandler *local_ui_handler;
	
	
	g_assert (NAUTILUS_IS_CLIPBOARD_INFO(user_data));
	info = NAUTILUS_CLIPBOARD_INFO(user_data);

	g_assert (BONOBO_IS_CONTROL (nautilus_clipboard_info_get_bonobo_control (info)));
	local_ui_handler = bonobo_control_get_ui_handler (nautilus_clipboard_info_get_bonobo_control (info));
	bonobo_ui_handler_unset_container (local_ui_handler); 


}

static void
nautilus_component_bonobo_menu_cut_cb (BonoboUIHandler *uihandler,
				       gpointer data, const char *path)
{
	NautilusClipboardInfo *info;
	
	g_assert (NAUTILUS_IS_CLIPBOARD_INFO (data));
	info = NAUTILUS_CLIPBOARD_INFO (data);
	
	g_assert (GTK_IS_EDITABLE (nautilus_clipboard_info_get_clipboard_owner (info)));
	gtk_editable_cut_clipboard (GTK_EDITABLE (nautilus_clipboard_info_get_clipboard_owner (info)));

}



static void
nautilus_component_bonobo_menu_copy_cb (BonoboUIHandler *uihandler,
					gpointer data, const char *path)
{
	NautilusClipboardInfo *info;
	
	g_assert (NAUTILUS_IS_CLIPBOARD_INFO (data));
	info = NAUTILUS_CLIPBOARD_INFO (data);
	
	g_assert (GTK_IS_EDITABLE (nautilus_clipboard_info_get_clipboard_owner (info)));
	gtk_editable_copy_clipboard (GTK_EDITABLE (nautilus_clipboard_info_get_clipboard_owner (info)));

}


static void
nautilus_component_bonobo_menu_paste_cb (BonoboUIHandler *uihandler,
					 gpointer data, const char *path)
{
		NautilusClipboardInfo *info;
	
	g_assert (NAUTILUS_IS_CLIPBOARD_INFO (data));
	info = NAUTILUS_CLIPBOARD_INFO (data);
	
	g_assert (GTK_IS_EDITABLE (nautilus_clipboard_info_get_clipboard_owner (info)));
	gtk_editable_paste_clipboard (GTK_EDITABLE (nautilus_clipboard_info_get_clipboard_owner (info)));



}



static void
nautilus_component_bonobo_menu_clear_cb (BonoboUIHandler *uihandler,
					gpointer data, const char *path)
{
	NautilusClipboardInfo *info;
	
	g_assert (NAUTILUS_IS_CLIPBOARD_INFO (data));
	info = NAUTILUS_CLIPBOARD_INFO (data);
	
	g_assert (GTK_IS_EDITABLE (nautilus_clipboard_info_get_clipboard_owner (info)));
	/* A negative index deletes until the end of the string */
	gtk_editable_delete_text (GTK_EDITABLE (nautilus_clipboard_info_get_clipboard_owner (info)),
				  0, -1);

}
