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

#include <config.h>
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
#include <libnautilus/nautilus-gtk-extensions.h>
#include <libnautilus/nautilus-gtk-macros.h>
#include <libnautilus/nautilus-alloc.h>

#define DISPLAY_TIMEOUT_INTERVAL_MSECS 500

enum 
{
	ADD_ENTRY,
	BEGIN_ADDING_ENTRIES,
	CLEAR,
	DONE_ADDING_ENTRIES,
	BEGIN_LOADING,
	APPEND_BACKGROUND_CONTEXT_MENU_ITEMS,
	LAST_SIGNAL
};

static guint fm_directory_view_signals[LAST_SIGNAL];

struct _FMDirectoryViewDetails
{
	NautilusContentViewFrame *view_frame;
	NautilusDirectory *model;
	
	guint display_selection_idle_id;
	
	guint display_pending_timeout_id;
	guint display_pending_idle_id;
	
	guint add_files_handler_id;
	
	NautilusFileList *pending_list;

	gboolean loading;
};

/* forward declarations */
static gint display_selection_info_idle_cb 	(gpointer data);
static void display_selection_info 		(FMDirectoryView *view);
static void fm_directory_view_initialize_class	(FMDirectoryViewClass *klass);
static void fm_directory_view_initialize 	(FMDirectoryView *view);
static void fm_directory_view_destroy 		(GtkObject *object);
static void fm_directory_view_append_background_context_menu_items 
						(FMDirectoryView *view,
						 GtkMenu *menu);
static void fm_directory_view_real_append_background_context_menu_items 		
						(FMDirectoryView *view, GtkMenu *menu);
static void append_item_context_menu_items 	(FMDirectoryView *view, 
						 GtkMenu *menu, 
						 NautilusFile *file);
static GtkMenu *create_item_context_menu        (FMDirectoryView *view,
						 NautilusFile *file);
static GtkMenu *create_background_context_menu  (FMDirectoryView *view);
static void stop_location_change_cb 		(NautilusViewFrame *view_frame, 
						 FMDirectoryView *directory_view);
static void notify_location_change_cb 		(NautilusViewFrame *view_frame, 
						 Nautilus_NavigationInfo *nav_context, 
						 FMDirectoryView *directory_view);
static void open_cb 				(GtkMenuItem *item, NautilusFile *file);
static void open_in_new_window_cb 		(GtkMenuItem *item, NautilusFile *file);
static void select_all_cb                       (GtkMenuItem *item, FMDirectoryView *directory_view);
static void zoom_in_cb                          (GtkMenuItem *item, FMDirectoryView *directory_view);
static void zoom_out_cb                         (GtkMenuItem *item, FMDirectoryView *directory_view);

static void schedule_idle_display_of_pending_files      (FMDirectoryView *view);
static void unschedule_idle_display_of_pending_files    (FMDirectoryView *view);
static void schedule_timeout_display_of_pending_files   (FMDirectoryView *view);
static void unschedule_timeout_display_of_pending_files (FMDirectoryView *view);
static void unschedule_display_of_pending_files         (FMDirectoryView *view);

static void disconnect_model_handlers                   (FMDirectoryView *view);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMDirectoryView, fm_directory_view, GTK_TYPE_SCROLLED_WINDOW)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, add_entry)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, clear)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, get_selection)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, select_all)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, bump_zoom_level)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, can_zoom_in)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, can_zoom_out)

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
	fm_directory_view_signals[APPEND_BACKGROUND_CONTEXT_MENU_ITEMS] =
		gtk_signal_new ("append_background_context_menu_items",
       				GTK_RUN_FIRST,
                    		object_class->type,
                    		GTK_SIGNAL_OFFSET (FMDirectoryViewClass, append_background_context_menu_items),
		    		gtk_marshal_NONE__POINTER,
		    		GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	klass->append_background_context_menu_items = fm_directory_view_real_append_background_context_menu_items;

	/* Function pointers that subclasses must override */

	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, add_entry);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, clear);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, get_selection);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, select_all);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, bump_zoom_level);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, can_zoom_in);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, fm_directory_view, can_zoom_out);
}

static void
fm_directory_view_initialize (FMDirectoryView *directory_view)
{
	directory_view->details = g_new0 (FMDirectoryViewDetails, 1);
	
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (directory_view),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (directory_view), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (directory_view), NULL);

	directory_view->details->view_frame = NAUTILUS_CONTENT_VIEW_FRAME
		(gtk_widget_new (nautilus_content_view_frame_get_type (), NULL));

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

	if (view->details->model != NULL) {
		disconnect_model_handlers (view);
		gtk_object_unref (GTK_OBJECT (view->details->model));
	}

	if (view->details->display_selection_idle_id != 0)
		gtk_idle_remove (view->details->display_selection_idle_id);

	unschedule_display_of_pending_files (view);

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
notify_location_change_cb (NautilusViewFrame *view_frame,
			   Nautilus_NavigationInfo *navigation_context,
			   FMDirectoryView *directory_view)
{
	fm_directory_view_load_uri (directory_view, navigation_context->requested_uri);
}

static void
stop_location_change_cb (NautilusViewFrame *view_frame,
			 FMDirectoryView *directory_view)
{
	fm_directory_view_stop (directory_view);
}



static void
stop_load (FMDirectoryView *view, gboolean error)
{
	Nautilus_ProgressRequestInfo progress;
	
	if (!view->details->loading) {
		g_assert (!error);
		return;
	}

	nautilus_directory_stop_monitoring (view->details->model);
	
	memset(&progress, 0, sizeof(progress));
	progress.amount = 100.0;
	progress.type = error ? Nautilus_PROGRESS_DONE_ERROR : Nautilus_PROGRESS_DONE_OK;
	nautilus_view_frame_request_progress_change
		(NAUTILUS_VIEW_FRAME (view->details->view_frame), &progress);
}



/* handle the "select all" menu command */

static void
select_all_cb(GtkMenuItem *item, FMDirectoryView *directory_view)
{
	fm_directory_view_select_all (directory_view);
}

/* handle the zoom in/out menu items */

static void
zoom_in_cb(GtkMenuItem *item, FMDirectoryView *directory_view)
{
	fm_directory_view_bump_zoom_level (directory_view, 1);
}

static void
zoom_out_cb(GtkMenuItem *item, FMDirectoryView *directory_view)
{
	fm_directory_view_bump_zoom_level (directory_view, -1);
}

static gboolean
display_pending_files (FMDirectoryView *view)
{
	NautilusFileList *pending_list;
	NautilusFileList *p;

	if (view->details->model != NULL
	    && nautilus_directory_are_all_files_seen (view->details->model))
		stop_load (view, FALSE);

	pending_list = view->details->pending_list;
	if (pending_list == NULL)
		return FALSE;
	view->details->pending_list = NULL;

	fm_directory_view_begin_adding_entries (view);

	for (p = pending_list; p != NULL; p = p->next) {
		fm_directory_view_add_entry (view, p->data);
		nautilus_file_unref (p->data);
	}

	fm_directory_view_done_adding_entries (view);

	g_list_free (pending_list);

	return TRUE;
}

static gboolean
display_selection_info_idle_cb (gpointer data)
{
	FMDirectoryView *view;
	
	view = FM_DIRECTORY_VIEW (data);

	view->details->display_selection_idle_id = 0;

	display_selection_info (view);
	fm_directory_view_send_selection_change (view);

	return FALSE;
}

static gboolean
display_pending_idle_cb (gpointer data)
{
	/* Don't do another idle until we receive more files. */

	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (data);

	view->details->display_pending_idle_id = 0;

	display_pending_files (view);

	return FALSE;
}

static gboolean
display_pending_timeout_cb (gpointer data)
{
	/* Do another timeout if we displayed some files.
	 * Once we get all the files, we'll start using
	 * idle instead.
	 */

	FMDirectoryView *view;
	gboolean displayed_some;

	view = FM_DIRECTORY_VIEW (data);

	displayed_some = display_pending_files (view);
	if (displayed_some)
		return TRUE;

	view->details->display_pending_timeout_id = 0;
	return FALSE;
}



static void
schedule_idle_display_of_pending_files (FMDirectoryView *view)
{
	/* No need to schedule an idle if there's already one pending. */
	if (view->details->display_pending_idle_id != 0)
		return;

	/* An idle takes precedence over a timeout. */
	unschedule_timeout_display_of_pending_files (view);

	view->details->display_pending_idle_id =
		gtk_idle_add (display_pending_idle_cb, view);
}

static void
schedule_timeout_display_of_pending_files (FMDirectoryView *view)
{
	/* No need to schedule a timeout if there's already one pending. */
	if (view->details->display_pending_timeout_id != 0)
		return;

	/* An idle takes precedence over a timeout. */
	if (view->details->display_pending_idle_id != 0)
		return;

	view->details->display_pending_timeout_id =
		gtk_timeout_add (DISPLAY_TIMEOUT_INTERVAL_MSECS,
				 display_pending_timeout_cb, view);
}

static void
unschedule_idle_display_of_pending_files (FMDirectoryView *view)
{
	/* Get rid of idle if it's active. */
	if (view->details->display_pending_idle_id != 0) {
		g_assert (view->details->display_pending_timeout_id == 0);
		gtk_idle_remove (view->details->display_pending_idle_id);
		view->details->display_pending_idle_id = 0;
	}
}

static void
unschedule_timeout_display_of_pending_files (FMDirectoryView *view)
{
	/* Get rid of timeout if it's active. */
	if (view->details->display_pending_timeout_id != 0) {
		g_assert (view->details->display_pending_idle_id == 0);
		gtk_timeout_remove (view->details->display_pending_timeout_id);
		view->details->display_pending_timeout_id = 0;
	}
}

static void
unschedule_display_of_pending_files (FMDirectoryView *view)
{
	unschedule_idle_display_of_pending_files (view);
	unschedule_timeout_display_of_pending_files (view);
}

static void
add_files_cb (NautilusDirectory *directory,
	      NautilusFileList *files,
	      gpointer callback_data)
{
	FMDirectoryView *view;
	NautilusFileList *p;

	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (files != NULL);

	view = FM_DIRECTORY_VIEW (callback_data);

	g_assert (directory == view->details->model);

	/* Put the files on the pending list. */
	for (p = files; p != NULL; p = p->next)
		nautilus_file_ref (p->data);
	view->details->pending_list = g_list_concat
		(view->details->pending_list, g_list_copy (files));
	
	/* If we haven't see all the files yet, then we'll wait for the
	   timeout to fire. If we have seen all the files, then we'll use
	   an idle instead.
	*/
	if (nautilus_directory_are_all_files_seen (view->details->model))
		schedule_idle_display_of_pending_files (view);
	else
		schedule_timeout_display_of_pending_files (view);
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
	g_return_if_fail (NAUTILUS_IS_FILE (file));

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
void
fm_directory_view_bump_zoom_level (FMDirectoryView *view, gint zoom_increment)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	(* FM_DIRECTORY_VIEW_CLASS (GTK_OBJECT (view)->klass)->bump_zoom_level) (view, zoom_increment);
}

/**
 * fm_directory_view_can_zoom_in:
 *
 * Determine whether the view can be zoomed any closer.
 * @view: The zoomable FMDirectoryView.
 * 
 * Return value: TRUE if @view can be zoomed any closer, FALSE otherwise.
 * 
 **/
gboolean
fm_directory_view_can_zoom_in (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	return (* FM_DIRECTORY_VIEW_CLASS (GTK_OBJECT (view)->klass)->can_zoom_in) (view);
}

/**
 * fm_directory_view_can_zoom_out:
 *
 * Determine whether the view can be zoomed any further away.
 * @view: The zoomable FMDirectoryView.
 * 
 * Return value: TRUE if @view can be zoomed any further away, FALSE otherwise.
 * 
 **/
gboolean
fm_directory_view_can_zoom_out (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), FALSE);

	return (* FM_DIRECTORY_VIEW_CLASS (GTK_OBJECT (view)->klass)->can_zoom_out) (view);
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
open_cb (GtkMenuItem *item, NautilusFile *file)
{
	FMDirectoryView *directory_view;

	directory_view = FM_DIRECTORY_VIEW (gtk_object_get_data (GTK_OBJECT (item), "directory_view"));

	fm_directory_view_activate_entry (directory_view, file, FALSE);
}

static void
open_in_new_window_cb (GtkMenuItem *item, NautilusFile *file)
{
	FMDirectoryView *directory_view;

	directory_view = FM_DIRECTORY_VIEW (gtk_object_get_data (GTK_OBJECT (item), "directory_view"));

	fm_directory_view_activate_entry (directory_view, file, TRUE);
}


static void
fm_directory_view_real_append_background_context_menu_items (FMDirectoryView *view, 
							     GtkMenu *menu)
{
	GtkWidget *menu_item;

	menu_item = gtk_menu_item_new_with_label ("Select all");
	gtk_widget_show (menu_item);
	gtk_signal_connect(GTK_OBJECT (menu_item), "activate",
		           GTK_SIGNAL_FUNC (select_all_cb), view);
	gtk_menu_append (menu, menu_item);


	menu_item = gtk_menu_item_new_with_label ("Zoom in");	
	gtk_signal_connect(GTK_OBJECT (menu_item), "activate",
		           GTK_SIGNAL_FUNC (zoom_in_cb), view);

	gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);
	gtk_widget_set_sensitive (menu_item, fm_directory_view_can_zoom_in (view));

	menu_item = gtk_menu_item_new_with_label ("Zoom out");
	
	gtk_signal_connect(GTK_OBJECT (menu_item), "activate",
		           GTK_SIGNAL_FUNC (zoom_out_cb), view);
	
        gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);
	gtk_widget_set_sensitive (menu_item, fm_directory_view_can_zoom_out (view));
}

static void
append_item_context_menu_items (FMDirectoryView *view, GtkMenu *menu, NautilusFile *file)
{
	GtkWidget *menu_item;

	menu_item = gtk_menu_item_new_with_label ("Open");
	/* Store directory view in menu item so callback can access it. */
	gtk_object_set_data_full (GTK_OBJECT (menu_item), "directory_view",
				  view, (GtkDestroyNotify) gtk_object_unref);
	gtk_object_ref (GTK_OBJECT (view));
	gtk_signal_connect(GTK_OBJECT (menu_item), "activate",
		           GTK_SIGNAL_FUNC (open_cb), file);
	gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);

	menu_item = gtk_menu_item_new_with_label ("Open in New Window");
	/* Store directory view in menu item so callback can access it. */
	gtk_object_set_data_full (GTK_OBJECT (menu_item), "directory_view",
				  view, (GtkDestroyNotify) gtk_object_unref);
	gtk_object_ref (GTK_OBJECT (view));
	gtk_signal_connect(GTK_OBJECT (menu_item), "activate",
		           GTK_SIGNAL_FUNC (open_in_new_window_cb), file);
	gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);

	menu_item = gtk_menu_item_new_with_label ("Delete");
	gtk_widget_set_sensitive (menu_item, FALSE);
	gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);
}

/* FIXME - need better architecture for setting these. */

static GtkMenu *
create_item_context_menu (FMDirectoryView *view,
			  NautilusFile *file) 
{
	GtkMenu *menu;
	GtkWidget *menu_item;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (NAUTILUS_IS_FILE (file));
	
	menu = GTK_MENU (gtk_menu_new ());

	append_item_context_menu_items (view, menu, file);
	
	/* separator between item-specific and view-general menu items */
	menu_item = gtk_menu_item_new ();
	gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);

	/* Show commands not specific to this item also, since it might
	 * be hard (especially in list view) to find a place to click
	 * that's not on an item.
	 */
	fm_directory_view_append_background_context_menu_items (view, menu);

	return menu;
}

/**
 * fm_directory_view_append_background_context_menu_items:
 *
 * Add background menu items (i.e., those not dependent on a particular file)
 * to a context menu.
 * @view: An FMDirectoryView.
 * @menu: The menu being constructed. Could be a background menu or an item-specific
 * menu, because the background items are present in both.
 * 
 **/
static void
fm_directory_view_append_background_context_menu_items (FMDirectoryView *view,
							GtkMenu *menu)
{
	gtk_signal_emit (GTK_OBJECT (view), 
			 fm_directory_view_signals[APPEND_BACKGROUND_CONTEXT_MENU_ITEMS], 
			 menu);
}


/* FIXME - need a way for specific views to add custom commands here. */

static GtkMenu *
create_background_context_menu (FMDirectoryView *view)
{
	GtkMenu *menu;

	menu = GTK_MENU (gtk_menu_new ());
	fm_directory_view_append_background_context_menu_items (view, menu);
	
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
	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (NAUTILUS_IS_FILE (file));
	
	nautilus_pop_up_context_menu (create_item_context_menu (view, file));
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
	
	nautilus_pop_up_context_menu (create_background_context_menu (view));
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

	/* Schedule a display of the new selection. */
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
 * @request_new_window: Should this item be opened in a new window?
 * 
 **/
void
fm_directory_view_activate_entry (FMDirectoryView *view, 
				  NautilusFile *file,
				  gboolean request_new_window)
{
	Nautilus_NavigationRequestInfo request;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));
	g_return_if_fail (NAUTILUS_IS_FILE (file));

	request.requested_uri = nautilus_file_get_uri (file);
	request.new_window_default = Nautilus_V_FALSE;
	request.new_window_suggested = request_new_window ? 
				       Nautilus_V_TRUE : 
				       Nautilus_V_FALSE;
	request.new_window_enforced = Nautilus_V_UNKNOWN;
	nautilus_view_frame_request_location_change
		(NAUTILUS_VIEW_FRAME (view->details->view_frame), &request);

	g_free (request.requested_uri);
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
	Nautilus_ProgressRequestInfo progress;
	NautilusDirectory *old_model;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));
	g_return_if_fail (uri != NULL);

	fm_directory_view_stop (view);
	fm_directory_view_clear (view);

	disconnect_model_handlers (view);

	old_model = view->details->model;
	view->details->model = nautilus_directory_get (uri);
	if (old_model != NULL)
		gtk_object_unref (GTK_OBJECT (old_model));

	memset(&progress, 0, sizeof(progress));
	progress.type = Nautilus_PROGRESS_UNDERWAY;
	nautilus_view_frame_request_progress_change
		(NAUTILUS_VIEW_FRAME (view->details->view_frame), &progress);

	/* Tell interested parties that we've begun loading this directory now.
	 * Subclasses use this to know that the new metadata is now available.
	 */
	gtk_signal_emit (GTK_OBJECT (view), fm_directory_view_signals[BEGIN_LOADING]);

	schedule_timeout_display_of_pending_files (view);
	view->details->loading = TRUE;
	nautilus_directory_start_monitoring (view->details->model,
					     add_files_cb, view);

	/* Attach a handler to get any further files that show up as we
	 * load and sychronize. We won't miss any files because this
	 * signal is emitted from an idle routine and so we will be
	 * connected before the next time it is emitted.
	 */
	view->details->add_files_handler_id = gtk_signal_connect
		(GTK_OBJECT (view->details->model), 
		 "files_added",
		 GTK_SIGNAL_FUNC (add_files_cb),
		 view);
}

static void
disconnect_model_handlers (FMDirectoryView *view)
{
	if (view->details->add_files_handler_id != 0) {
		gtk_signal_disconnect (GTK_OBJECT (view->details->model),
				       view->details->add_files_handler_id);
		view->details->add_files_handler_id = 0;
	}
}

/**
 * fm_directory_view_select_all:
 *
 * select all the items in the view
 * 
 **/
void
fm_directory_view_select_all (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	(* FM_DIRECTORY_VIEW_CLASS (GTK_OBJECT (view)->klass)->select_all) (view);
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

	unschedule_display_of_pending_files (view);
	display_pending_files (view);
	stop_load (view, FALSE);
}
