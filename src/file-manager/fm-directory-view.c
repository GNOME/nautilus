/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* fm-directory-view.c
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
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fm-directory-view.h"

#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-directory-list.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus/nautilus-gtk-macros.h>

#ifndef g_alloca
#include <alloca.h>
#define g_alloca alloca
#endif

#define DISPLAY_TIMEOUT_INTERVAL 500
#define ENTRIES_PER_CB 1

enum 
{
	ADD_ENTRY,
	BEGIN_ADDING_ENTRIES,
	CLEAR,
	DONE_ADDING_ENTRIES,
	BEGIN_LOADING,
	LAST_SIGNAL
};

static guint fm_directory_view_signals[LAST_SIGNAL] = { 0 };

struct _FMDirectoryViewDetails
{
	NautilusContentViewFrame *view_frame;

	GnomeVFSDirectoryList *directory_list;
	GnomeVFSDirectoryListPosition current_position;

	guint display_selection_idle_id;
	guint display_timeout_id;

	GnomeVFSAsyncHandle *vfs_async_handle;
	GnomeVFSURI *uri;

	NautilusDirectory *model;

	GtkMenu *background_context_menu;
        
        GtkWidget *zoom_in_item;
        GtkWidget *zoom_out_item;
};

/* forward declarations */
static gint display_selection_info_idle_cb 	(gpointer data);
static void display_selection_info 		(FMDirectoryView *view);
static void fm_directory_view_initialize_class	(FMDirectoryViewClass *klass);
static void fm_directory_view_initialize 	(FMDirectoryView *view);
static void fm_directory_view_destroy 		(GtkObject *object);
static void append_background_items 		(FMDirectoryView *view, GtkMenu *menu);
static GtkMenu *create_item_context_menu        (FMDirectoryView *view,
						 NautilusFile *file);
static GtkMenu *create_background_context_menu  (FMDirectoryView *view);
static void stop_location_change_cb 		(NautilusViewFrame *view_frame, 
						 FMDirectoryView *directory_view);
static void notify_location_change_cb 		(NautilusViewFrame *view_frame, 
						 Nautilus_NavigationInfo *nav_context, 
						 FMDirectoryView *directory_view);
static void zoom_in_cb                          (GtkMenuItem *item, FMDirectoryView *directory_view);
static void zoom_out_cb                         (GtkMenuItem *item, FMDirectoryView *directory_view);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMDirectoryView, fm_directory_view, GTK_TYPE_SCROLLED_WINDOW)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, add_entry)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, clear)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, get_selection)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, bump_zoom_level)

static void
fm_directory_view_initialize_class (FMDirectoryViewClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = fm_directory_view_destroy;

	fm_directory_view_signals[CLEAR] =
		gtk_signal_new ("clear",
       				GTK_RUN_LAST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (FMDirectoryViewClass, clear),
		    		gtk_marshal_NONE__NONE,
		    		GTK_TYPE_NONE, 0);
	fm_directory_view_signals[BEGIN_ADDING_ENTRIES] =
		gtk_signal_new ("begin_adding_entries",
       				GTK_RUN_LAST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (FMDirectoryViewClass, begin_adding_entries),
		    		gtk_marshal_NONE__NONE,
		    		GTK_TYPE_NONE, 0);
	fm_directory_view_signals[ADD_ENTRY] =
		gtk_signal_new ("add_entry",
       				GTK_RUN_LAST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (FMDirectoryViewClass, add_entry),
		    		gtk_marshal_NONE__POINTER,
		    		GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	fm_directory_view_signals[DONE_ADDING_ENTRIES] =
		gtk_signal_new ("done_adding_entries",
       				GTK_RUN_LAST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (FMDirectoryViewClass, done_adding_entries),
		    		gtk_marshal_NONE__NONE,
		    		GTK_TYPE_NONE, 0);
	fm_directory_view_signals[BEGIN_LOADING] =
		gtk_signal_new ("begin_loading",
       				GTK_RUN_LAST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (FMDirectoryViewClass, begin_loading),
		    		gtk_marshal_NONE__NONE,
		    		GTK_TYPE_NONE, 0);

	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, add_entry);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, clear);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, get_selection);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, bump_zoom_level);
}

static void
fm_directory_view_initialize (FMDirectoryView *directory_view)
{
	directory_view->details = g_new0 (FMDirectoryViewDetails, 1);
	
#if 0
	gtk_scroll_frame_set_policy (GTK_SCROLL_FRAME (directory_view),
				     GTK_POLICY_AUTOMATIC,
				     GTK_POLICY_AUTOMATIC);
	gtk_scroll_frame_set_shadow_type (GTK_SCROLL_FRAME (directory_view), GTK_SHADOW_IN);
#else
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (directory_view),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (directory_view), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (directory_view), NULL);

#endif

	directory_view->details->background_context_menu = create_background_context_menu(directory_view);

	directory_view->details->view_frame = NAUTILUS_CONTENT_VIEW_FRAME
		(gtk_widget_new (nautilus_content_view_frame_get_type(), NULL));

	gtk_signal_connect (GTK_OBJECT (directory_view->details->view_frame), 
			    "stop_location_change",
			    GTK_SIGNAL_FUNC (stop_location_change_cb),
			    directory_view);

	gtk_signal_connect (GTK_OBJECT (directory_view->details->view_frame), 
			    "notify_location_change",
			    GTK_SIGNAL_FUNC (notify_location_change_cb), 
			    directory_view);


	gtk_widget_show (GTK_WIDGET (directory_view));

	gtk_container_add (GTK_CONTAINER (directory_view->details->view_frame),
			   GTK_WIDGET (directory_view));
}

static void
fm_directory_view_destroy (GtkObject *object)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (object);

	if (view->details->directory_list != NULL)
		gnome_vfs_directory_list_destroy (view->details->directory_list);

	if (view->details->uri != NULL)
		gnome_vfs_uri_unref (view->details->uri);

	if (view->details->vfs_async_handle != NULL)
		gnome_vfs_async_cancel (view->details->vfs_async_handle);

	if (view->details->display_timeout_id != 0)
		gtk_timeout_remove (view->details->display_timeout_id);

	if (view->details->model != NULL)
		gtk_object_unref (GTK_OBJECT (view->details->model));

	if (view->details->background_context_menu != NULL)
		gtk_object_unref(GTK_OBJECT(view->details->background_context_menu));

	g_free (view->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}



/**
 * display_selection_info:
 *
 * Display information about the current selection, and notify the view frame of the changed selection.
 * @view: FMDirectoryView for which to display selection info.
 * 
 **/
static void
display_selection_info (FMDirectoryView *view)
{
	NautilusFileList *selection;
	GnomeVFSFileSize size;
	guint count;
	NautilusFileList *p;
	char *first_item_name;
	Nautilus_StatusRequestInfo sri;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	selection = fm_directory_view_get_selection (view);
	
	count = 0;
	size = 0;
	first_item_name = NULL;
	for (p = selection; p != NULL; p = p->next) {
		NautilusFile *file;

		file = p->data;
		count++;
		size += nautilus_file_get_size (file);
		if (first_item_name == NULL)
			first_item_name = nautilus_file_get_name (file);
	}
		
	g_list_free (selection);
	
	memset(&sri, 0, sizeof(sri));

	if (count == 0) 
	{
	        sri.status_string = "";
	}
	else
	{
		char *size_string;

		size_string = gnome_vfs_file_size_to_string (size);
		if (count == 1)
		{
			g_assert (first_item_name != NULL && strlen (first_item_name) > 0);
			
			sri.status_string = g_strdup_printf (_("\"%s\" selected -- %s"), 
							     first_item_name, 
							     size_string);
		}
		else
		{
			sri.status_string = g_strdup_printf (_("%d items selected -- %s"), 
							     count, 
							     size_string);
		}
		g_free (size_string);
	}

	g_free (first_item_name);

	nautilus_view_frame_request_status_change
		(NAUTILUS_VIEW_FRAME (view->details->view_frame), &sri);
}

static void
fm_directory_view_send_selection_change (FMDirectoryView *view)
{
	Nautilus_SelectionRequestInfo request;
	NautilusFileList *selection;
	NautilusFileList *p;
	int i;

	memset (&request, 0, sizeof (request));

	/* Collect a list of URIs. */
	selection = fm_directory_view_get_selection (view);
	request.selected_uris._buffer = g_alloca (g_list_length (selection) * sizeof (char *));
	for (p = selection; p != NULL; p = p->next)
		request.selected_uris._buffer[request.selected_uris._length++]
			= nautilus_file_get_uri (p->data);
	g_list_free (selection);

	/* Send the selection change. */
	nautilus_view_frame_request_selection_change
		(NAUTILUS_VIEW_FRAME (view->details->view_frame), &request);

	/* Free the URIs. */
	for (i = 0; i < request.selected_uris._length; i++)
		g_free (request.selected_uris._buffer[i]);
}



static void
notify_location_change_cb (NautilusViewFrame *view_frame, Nautilus_NavigationInfo *nav_context, FMDirectoryView *directory_view)
{
	fm_directory_view_load_uri(directory_view, nav_context->requested_uri);
}

static void
stop_location_change_cb (NautilusViewFrame *view_frame, FMDirectoryView *directory_view)
{
	fm_directory_view_stop(directory_view);
}



static void
stop_load (FMDirectoryView *view, gboolean error)
{
	Nautilus_ProgressRequestInfo pri;

	if (view->details->vfs_async_handle != NULL) {
		gnome_vfs_async_cancel (view->details->vfs_async_handle);
		view->details->vfs_async_handle = NULL;
	}

	if (view->details->display_timeout_id != 0) {
		gtk_timeout_remove (view->details->display_timeout_id);
		view->details->display_timeout_id = 0;
	}

	memset(&pri, 0, sizeof(pri));
	pri.amount = 100.0;
	pri.type = error ? Nautilus_PROGRESS_DONE_ERROR : Nautilus_PROGRESS_DONE_OK;
	
	nautilus_view_frame_request_progress_change
		(NAUTILUS_VIEW_FRAME (view->details->view_frame), &pri);
}



/* handle the zoom in/out menu items */

static void
zoom_in_cb(GtkMenuItem *item, FMDirectoryView *directory_view)
{
    
   gboolean can_zoom_in = fm_directory_view_bump_zoom_level (directory_view, 1);
   gtk_widget_set_sensitive(directory_view->details->zoom_in_item, can_zoom_in);
   gtk_widget_set_sensitive(directory_view->details->zoom_out_item, TRUE);
}

static void
zoom_out_cb(GtkMenuItem *item, FMDirectoryView *directory_view)
{
     gboolean can_zoom_out = fm_directory_view_bump_zoom_level (directory_view, -1);
     gtk_widget_set_sensitive(directory_view->details->zoom_out_item, can_zoom_out);
     gtk_widget_set_sensitive(directory_view->details->zoom_in_item, TRUE);
     
}

static void
display_pending_entries (FMDirectoryView *view)
{
	fm_directory_view_begin_adding_entries (view);

	while (view->details->current_position != GNOME_VFS_DIRECTORY_LIST_POSITION_NONE)
	{
		GnomeVFSFileInfo *info;

		info = gnome_vfs_directory_list_get (view->details->directory_list,
						     view->details->current_position);

		fm_directory_view_add_entry (view, nautilus_directory_new_file (view->details->model, info));

		view->details->current_position = gnome_vfs_directory_list_position_next
						       (view->details->current_position);
	}

	fm_directory_view_done_adding_entries (view);
}

static gint
display_selection_info_idle_cb (gpointer data)
{
	FMDirectoryView *view;
	
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (data), FALSE);

	view = FM_DIRECTORY_VIEW (data);

	display_selection_info (view);
	fm_directory_view_send_selection_change (view);

	view->details->display_selection_idle_id = 0;

	return FALSE;
}

static gboolean
display_timeout_cb (gpointer data)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (data), TRUE);
	
	display_pending_entries (FM_DIRECTORY_VIEW (data));

	return TRUE;
}


static void
directory_load_cb (GnomeVFSAsyncHandle *handle,
		   GnomeVFSResult result,
		   GnomeVFSDirectoryList *list,
		   guint entries_read,
		   gpointer callback_data)
{
	FMDirectoryView *view;

	g_assert(entries_read <= ENTRIES_PER_CB);

	view = FM_DIRECTORY_VIEW (callback_data);

	if (view->details->directory_list == NULL) {
		if (result == GNOME_VFS_OK || result == GNOME_VFS_ERROR_EOF) {

			fm_directory_view_begin_loading (view);

			view->details->directory_list = list;

			g_assert (view->details->current_position
				== GNOME_VFS_DIRECTORY_LIST_POSITION_NONE);

			if (result != GNOME_VFS_ERROR_EOF)
				view->details->display_timeout_id
					= gtk_timeout_add
						(DISPLAY_TIMEOUT_INTERVAL,
						 display_timeout_cb,
						 view);
		} else if (entries_read == 0) {
			/*
			gtk_signal_emit (GTK_OBJECT (view),
					 signals[OPEN_FAILED]);
			*/
		}
	}

	if(view->details->current_position == GNOME_VFS_DIRECTORY_LIST_POSITION_NONE && list)
		view->details->current_position
			= gnome_vfs_directory_list_get_position (list);

	if (result == GNOME_VFS_ERROR_EOF) {
		display_pending_entries (view);
		stop_load (view, FALSE);
	} else if (result != GNOME_VFS_OK) {
		stop_load (view, TRUE);
	}
}


/**
 * fm_directory_view_clear:
 *
 * Emit the signal to clear the contents of the view. Subclasses must
 * override the signal handler for this signal. This is normally called
 * only by FMDirectoryView.
 * @view: FMDirectoryView to empty.
 * 
 **/
void
fm_directory_view_clear (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	gtk_signal_emit (GTK_OBJECT (view), fm_directory_view_signals[CLEAR]);
}

/**
 * fm_directory_view_begin_adding_entries:
 *
 * Emit the signal to prepare for adding a set of entries to the view. 
 * Subclasses might want to override the signal handler for this signal. 
 * This is normally called only by FMDirectoryView.
 * @view: FMDirectoryView that will soon have new entries added.
 * 
 **/
void
fm_directory_view_begin_adding_entries (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	gtk_signal_emit (GTK_OBJECT (view), fm_directory_view_signals[BEGIN_ADDING_ENTRIES]);
}

/**
 * fm_directory_view_add_entry:
 *
 * Emit the signal to add one entry to the view. Subclasses must
 * override the signal handler for this signal. This is normally called
 * only by FMDirectoryView.
 * @view: FMDirectoryView to add entry to.
 * @file: NautilusFile describing entry to add.
 * 
 **/
void
fm_directory_view_add_entry (FMDirectoryView *view, NautilusFile *file)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	gtk_signal_emit (GTK_OBJECT (view), fm_directory_view_signals[ADD_ENTRY], file);
}

/**
 * fm_directory_view_done_adding_entries:
 *
 * Emit the signal to clean up after adding a set of entries to the view. 
 * Subclasses might want to override the signal handler for this signal. 
 * This is normally called only by FMDirectoryView.
 * @view: FMDirectoryView that has just had new entries added.
 * 
 **/
void
fm_directory_view_done_adding_entries (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	gtk_signal_emit (GTK_OBJECT (view), fm_directory_view_signals[DONE_ADDING_ENTRIES]);
}

/**
 * fm_directory_view_begin_loading:
 *
 * Emit the signal to prepare for loading the contents of a new location. 
 * Subclasses might want to override the signal handler for this signal. 
 * This is normally called only by FMDirectoryView.
 * @view: FMDirectoryView that is switching to view a new location.
 * 
 **/
void
fm_directory_view_begin_loading (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	gtk_signal_emit (GTK_OBJECT (view), fm_directory_view_signals[BEGIN_LOADING]);
}

/**
 * fm_directory_view_bump_zoom_level:
 *
 * bump the current zoom level by invoking the relevant subclass through the slot
 * 
 **/
gboolean
fm_directory_view_bump_zoom_level (FMDirectoryView *view, gint zoom_increment)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	return (* FM_DIRECTORY_VIEW_CLASS (GTK_OBJECT (view)->klass)->bump_zoom_level) (view, zoom_increment);
}

/**
 * fm_directory_view_get_selection:
 *
 * Get a list of NautilusFile pointers that represents the
 * currently-selected items in this view. Subclasses must override
 * the signal handler for the 'get_selection' signal. Callers are
 * responsible for g_free-ing the list (but not its data).
 * @view: FMDirectoryView whose selected items are of interest.
 * 
 * Return value: GList of NautilusFile pointers representing the selection.
 * 
 **/
GList *
fm_directory_view_get_selection (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);

	return (* FM_DIRECTORY_VIEW_CLASS (GTK_OBJECT (view)->klass)->get_selection) (view);
}

/**
 * fm_directory_view_get_view_frame:
 *
 * Get the NautilusContentViewFrame for this FMDirectoryView.
 * This is normally called only by the embedding framework.
 * @view: FMDirectoryView of interest.
 * 
 * Return value: NautilusContentViewFrame for this view.
 * 
 **/
NautilusContentViewFrame *
fm_directory_view_get_view_frame (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);

	return view->details->view_frame;
}

/**
 * fm_directory_view_get_model:
 *
 * Get the model for this FMDirectoryView.
 * @view: FMDirectoryView of interest.
 * 
 * Return value: NautilusDirectory for this view.
 * 
 **/
NautilusDirectory *
fm_directory_view_get_model (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);

	return view->details->model;
}

static void
popup_context_menu (GtkMenu *menu)
{
	gtk_menu_popup (menu, NULL, NULL, NULL,
			NULL, 3, GDK_CURRENT_TIME);
}

static void
popup_temporary_context_menu (GtkMenu *menu)
{
	gtk_object_ref (GTK_OBJECT(menu));
	gtk_object_sink (GTK_OBJECT(menu));

	popup_context_menu (menu);

	gtk_object_unref (GTK_OBJECT(menu));
}

static void
append_background_items (FMDirectoryView *view, GtkMenu *menu)
{
	GtkWidget *menu_item;

	menu_item = gtk_menu_item_new_with_label ("Select all");
	gtk_widget_set_sensitive (menu_item, FALSE);
	gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);


	menu_item = gtk_menu_item_new_with_label ("Zoom in");	
	gtk_signal_connect(GTK_OBJECT (menu_item), "activate",
		           GTK_SIGNAL_FUNC (zoom_in_cb), view);

	gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);
        view->details->zoom_in_item = menu_item;

       
	menu_item = gtk_menu_item_new_with_label ("Zoom out");
	
	gtk_signal_connect(GTK_OBJECT (menu_item), "activate",
		           GTK_SIGNAL_FUNC (zoom_out_cb), view);
	
        gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);
	view->details->zoom_out_item = menu_item;
}

/* FIXME - need better architecture for setting these. */

static GtkMenu *
create_item_context_menu (FMDirectoryView *view,
			  NautilusFile *file) 
{
	GtkMenu *menu;
	GtkWidget *menu_item;
	
	menu = GTK_MENU (gtk_menu_new ());

	menu_item = gtk_menu_item_new_with_label ("Open");
	gtk_widget_set_sensitive (menu_item, FALSE);
	gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);

	menu_item = gtk_menu_item_new_with_label ("Delete");
	gtk_widget_set_sensitive (menu_item, FALSE);
	gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);

	/* separator between item-specific and view-general menu items */
	menu_item = gtk_menu_item_new ();
	gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);

	/* Show commands not specific to this item also, since it might
	 * be hard (especially in list view) to find a place to click
	 * that's not on an item.
	 */
	append_background_items (view, menu);

	return menu;
}

/* FIXME - need a way for specific views to add custom commands here. */

static GtkMenu *
create_background_context_menu (FMDirectoryView *view)
{
	GtkMenu *menu;

	menu = GTK_MENU (gtk_menu_new ());

	append_background_items (view, menu);
	
	gtk_object_ref(GTK_OBJECT(menu));
	gtk_object_sink(GTK_OBJECT(menu));
	return menu;
}


/**
 * fm_directory_view_popup_item_context_menu
 *
 * Pop up a context menu appropriate to a specific view item at the last right click location.
 * @view: FMDirectoryView of interest.
 * @file: The model object for which a menu should be popped up.
 * 
 * Return value: NautilusDirectory for this view.
 * 
 **/

void 
fm_directory_view_popup_item_context_menu  (FMDirectoryView *view,
					    NautilusFile *file)
{
	GtkMenu *menu;
	g_assert (FM_IS_DIRECTORY_VIEW (view));
	/* g_assert (NAUTILUS_IS_FILE (file)); */
	
	menu = create_item_context_menu (view, file);

	popup_temporary_context_menu (menu);
}

/**
 * fm_directory_view_popup_background_context_menu
 *
 * Pop up a context menu appropriate to the view globally at the last right click location.
 * @view: FMDirectoryView of interest.
 * 
 * Return value: NautilusDirectory for this view.
 * 
 **/

void 
fm_directory_view_popup_background_context_menu  (FMDirectoryView *view)
{
	g_assert (FM_IS_DIRECTORY_VIEW (view));
	
	popup_context_menu (view->details->background_context_menu);
}


/**
 * fm_directory_view_notify_selection_changed:
 * 
 * Notify this view that the selection has changed. This is normally
 * called only by subclasses.
 * @view: FMDirectoryView whose selection has changed.
 * 
 **/
void
fm_directory_view_notify_selection_changed (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	if (view->details->display_selection_idle_id == 0)
		view->details->display_selection_idle_id
			= gtk_idle_add (display_selection_info_idle_cb,
					view);
}

/**
 * fm_directory_view_activate_entry:
 * 
 * Activate an entry in this view. This might involve switching the displayed
 * location for the current window, or launching an application. This is normally
 * called only by subclasses.
 * @view: FMDirectoryView in question.
 * @file: A NautilusFile representing the entry in this view to activate.
 * 
 **/
void
fm_directory_view_activate_entry (FMDirectoryView *view, NautilusFile *file)
{
	GnomeVFSURI *new_uri;
	char *name;
	char *new_uri_text;
	Nautilus_NavigationRequestInfo nri;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));
	g_return_if_fail (file != NULL);

	name = nautilus_file_get_name (file);
	new_uri = gnome_vfs_uri_append_path(view->details->uri, name);
	g_free (name);

	new_uri_text = gnome_vfs_uri_to_string (new_uri, GNOME_VFS_URI_HIDE_NONE);
	gnome_vfs_uri_unref (new_uri);

	nri.requested_uri = new_uri_text;
	nri.new_window_default = nri.new_window_suggested = Nautilus_V_FALSE;
	nri.new_window_enforced = Nautilus_V_UNKNOWN;
	nautilus_view_frame_request_location_change
		(NAUTILUS_VIEW_FRAME (view->details->view_frame), &nri);

	g_free (new_uri_text);
}

/**
 * fm_directory_view_load_uri:
 * 
 * Switch the displayed location to a new uri. If the uri is not valid,
 * the location will not be switched; user feedback will be provided instead.
 * @view: FMDirectoryView whose location will be changed.
 * @uri: A string representing the uri to switch to.
 * 
 **/
void
fm_directory_view_load_uri (FMDirectoryView *view,
			    const char *uri)
{
	GnomeVFSResult result;
	Nautilus_ProgressRequestInfo pri;
	NautilusDirectory *old_model;

	g_return_if_fail (view != NULL);
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));
	g_return_if_fail (uri != NULL);

	fm_directory_view_stop (view);

	old_model = view->details->model;
	view->details->model = nautilus_directory_get (uri);

	if (view->details->uri != NULL)
		gnome_vfs_uri_unref (view->details->uri);
	view->details->uri = gnome_vfs_uri_new (uri);

	if (view->details->directory_list != NULL)
		gnome_vfs_directory_list_destroy (view->details->directory_list);
	view->details->directory_list = NULL;
	view->details->current_position = GNOME_VFS_DIRECTORY_LIST_POSITION_NONE;

	fm_directory_view_clear (view);

	if (old_model != NULL)
		gtk_object_unref (GTK_OBJECT (old_model));

	memset(&pri, 0, sizeof(pri));
	pri.type = Nautilus_PROGRESS_UNDERWAY;
	pri.amount = 0;
	nautilus_view_frame_request_progress_change
		(NAUTILUS_VIEW_FRAME (view->details->view_frame), &pri);

	result = gnome_vfs_async_load_directory_uri
		(&view->details->vfs_async_handle, 	/* handle */
		 view->details->uri,			/* uri */
		 (GNOME_VFS_FILE_INFO_GETMIMETYPE	/* options */
		  | GNOME_VFS_FILE_INFO_FASTMIMETYPE
		  | GNOME_VFS_FILE_INFO_FOLLOWLINKS),
		 NULL, 					/* meta_keys */
		 NULL, 					/* sort_rules */
		 FALSE, 				/* reverse_order */
		 GNOME_VFS_DIRECTORY_FILTER_NONE, 	/* filter_type */
		 (GNOME_VFS_DIRECTORY_FILTER_NOSELFDIR  /* filter_options */
		  | GNOME_VFS_DIRECTORY_FILTER_NOPARENTDIR),
		 NULL, 					/* filter_pattern */
		 ENTRIES_PER_CB,			 /* items_per_notification */
		 directory_load_cb,	 		/* callback */
		 view);		 			/* callback_data */

	g_return_if_fail(result == GNOME_VFS_OK);
}

/**
 * fm_directory_view_stop:
 * 
 * Stop the current ongoing process, such as switching to a new uri.
 * @view: FMDirectoryView in question.
 * 
 **/
void
fm_directory_view_stop (FMDirectoryView *view)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	if (view->details->vfs_async_handle == NULL)
		return;

	display_pending_entries (view);
	stop_load (view, FALSE);
}