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
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-directory-list.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus/nautilus-gtk-macros.h>

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
static GtkScrolledWindowClass *parent_class = NULL;

struct _FMDirectoryViewDetails
{
	NautilusContentViewFrame *view_frame;

	GnomeVFSDirectoryList *directory_list;
	GnomeVFSDirectoryListPosition current_position;

	guint display_selection_idle_id;
	guint display_timeout_id;

	GnomeVFSAsyncHandle *vfs_async_handle;
	GnomeVFSURI *uri;
};

/* forward declarations */
static gint display_selection_info_idle_cb 	(gpointer data);
static void display_selection_info 		(FMDirectoryView *view);
static void fm_directory_view_initialize_class	(gpointer klass);
static void fm_directory_view_initialize 	(gpointer object, gpointer klass);
static void fm_directory_view_destroy 		(GtkObject *object);
static void stop_location_change_cb 		(NautilusViewFrame *view_frame, 
						 FMDirectoryView *directory_view);
static void notify_location_change_cb 		(NautilusViewFrame *view_frame, 
						 Nautilus_NavigationInfo *nav_context, 
						 FMDirectoryView *directory_view);

NAUTILUS_DEFINE_GET_TYPE_FUNCTION (FMDirectoryView, fm_directory_view, GTK_TYPE_SCROLLED_WINDOW)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, add_entry)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, clear)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, get_selection)

static void
fm_directory_view_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	parent_class = gtk_type_class (gtk_type_parent(object_class->type));
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

	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (FM_DIRECTORY_VIEW_CLASS,
					      klass,
					      fm_directory_view,
					      add_entry);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (FM_DIRECTORY_VIEW_CLASS,
					      klass,
					      fm_directory_view,
					      clear);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (FM_DIRECTORY_VIEW_CLASS,
					      klass,
					      fm_directory_view,
					      get_selection);
}

static void
fm_directory_view_initialize (gpointer object, gpointer klass)
{
	FMDirectoryView *directory_view;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW (object));

	directory_view = FM_DIRECTORY_VIEW (object);

	directory_view->details = g_new0 (FMDirectoryViewDetails, 1);

#if 0
	gtk_scroll_frame_set_policy (GTK_SCROLL_FRAME(directory_view),
				     GTK_POLICY_AUTOMATIC,
				     GTK_POLICY_AUTOMATIC);
	gtk_scroll_frame_set_shadow_type (GTK_SCROLL_FRAME(directory_view), GTK_SHADOW_IN);
#else
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(directory_view),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW(directory_view), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW(directory_view), NULL);

#endif

	directory_view->details->view_frame = 
		NAUTILUS_CONTENT_VIEW_FRAME (gtk_widget_new 
					     (nautilus_content_view_frame_get_type(), 
					      NULL));

	gtk_signal_connect (GTK_OBJECT(directory_view->details->view_frame), 
			    "stop_location_change",
			    GTK_SIGNAL_FUNC (stop_location_change_cb),
			    directory_view);

	gtk_signal_connect (GTK_OBJECT(directory_view->details->view_frame), 
			    "notify_location_change",
			    GTK_SIGNAL_FUNC (notify_location_change_cb), 
			    directory_view);

	gtk_widget_show(GTK_WIDGET(directory_view));

	gtk_container_add(GTK_CONTAINER(directory_view->details->view_frame),
			  GTK_WIDGET(directory_view));
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

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}



/**
 * display_selection_info:
 *
 * Display textual information about the current selection.
 * @view: FMDirectoryView for which to display selection info.
 * 
 **/
static void
display_selection_info (FMDirectoryView *view)
{
	GList *selection;
	GnomeVFSFileSize size;
	guint count;
	gchar *size_string, *msg;
	GList *p;
	Nautilus_StatusRequestInfo sri;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	selection = fm_directory_view_get_selection (view);
	
	count = 0;
	size = 0;
	for (p = selection; p != NULL; p = p->next) {
		GnomeVFSFileInfo *info;

		info = p->data;
		count++;
		size += info->size;
	}

	g_list_free (selection);

	if (count == 0) {
	        memset(&sri, 0, sizeof(sri));
	        sri.status_string = "";
	        nautilus_view_frame_request_status_change 
	        	(NAUTILUS_VIEW_FRAME (view->details->view_frame), &sri);
		return;
	}

	size_string = gnome_vfs_file_size_to_string (size);
	if (count == 1)
		msg = g_strdup_printf (_("1 item selected -- %s"), size_string);
	else
		msg = g_strdup_printf (_("%d items selected -- %s"), count, size_string);
	g_free (size_string);

	memset(&sri, 0, sizeof(sri));
	sri.status_string = msg;
	nautilus_view_frame_request_status_change
		(NAUTILUS_VIEW_FRAME (view->details->view_frame), &sri);

        g_free (msg);
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




/**
 * fm_directory_view_populate:
 *
 * Fill view with entries for current location, after emptying any old contents.
 * This is normally called only by FMDirectoryView and subclasses.
 * @view: FMDirectoryView to fill.
 * 
 **/
void
fm_directory_view_populate (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	/* Always start from scratch */
	fm_directory_view_clear (view);

	if (view->details->directory_list != NULL) 
	{
		GnomeVFSDirectoryListPosition *position;

		position = gnome_vfs_directory_list_get_first_position
			(view->details->directory_list);

		fm_directory_view_begin_adding_entries (view);

		while (position != view->details->current_position) {
			GnomeVFSFileInfo *info;

			info = gnome_vfs_directory_list_get
				(view->details->directory_list, position);

			fm_directory_view_add_entry (view, info);

			position = gnome_vfs_directory_list_position_next
				(position);
		}

		fm_directory_view_done_adding_entries (view);
	}
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

		fm_directory_view_add_entry (view, info);

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
 * @info: GnomeVFSFileInfo describing entry to add.
 * 
 **/
void
fm_directory_view_add_entry (FMDirectoryView *view, GnomeVFSFileInfo *info)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	gtk_signal_emit (GTK_OBJECT (view), fm_directory_view_signals[ADD_ENTRY], info);
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
 * fm_directory_view_get_selection:
 *
 * Get a list of GnomeVFSFileInfo pointers that represents the
 * currently-selected items in this view. Subclasses must override
 * the signal handler for the 'get_selection' signal. Callers are
 * responsible for g_free-ing the list (but not its data).
 * @view: FMDirectoryView whose selected items are of interest.
 * 
 * Return value: GList of GnomeVFSFileInfo pointers representing the selection.
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
 * fm_directory_view_get_uri:
 * 
 * Get the GnomeVFSURI representing this view's current location.
 * Callers must not modify the returned object.
 * @view: FMDirectoryView of interest.
 * 
 * Return value: uri for this view.
 * 
 **/
GnomeVFSURI *
fm_directory_view_get_uri (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);

	return view->details->uri;
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
 * @info: A GnomeVFSFileInfo representing the entry in this view to activate.
 * 
 **/
void
fm_directory_view_activate_entry (FMDirectoryView *view, GnomeVFSFileInfo *info)
{
	GnomeVFSURI *new_uri;
	Nautilus_NavigationRequestInfo nri;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));
	g_return_if_fail (info != NULL);

	new_uri = gnome_vfs_uri_append_path (view->details->uri, info->name);
	nri.requested_uri = gnome_vfs_uri_to_string(new_uri, 0);
	nri.new_window_default = nri.new_window_suggested = Nautilus_V_FALSE;
	nri.new_window_enforced = Nautilus_V_UNKNOWN;
	nautilus_view_frame_request_location_change
		(NAUTILUS_VIEW_FRAME(view->details->view_frame), &nri);
	g_free(nri.requested_uri);
	gnome_vfs_uri_unref (new_uri);	
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
	static GnomeVFSDirectorySortRule sort_rules[] = {
		GNOME_VFS_DIRECTORY_SORT_DIRECTORYFIRST,
		GNOME_VFS_DIRECTORY_SORT_BYNAME,
		GNOME_VFS_DIRECTORY_SORT_NONE
	};			/* FIXME */
	GnomeVFSResult result;
	Nautilus_ProgressRequestInfo pri;

	g_return_if_fail (view != NULL);
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));
	g_return_if_fail (uri != NULL);

	fm_directory_view_stop (view);

	fm_directory_view_clear (view);

	if (view->details->uri != NULL)
		gnome_vfs_uri_unref (view->details->uri);
	view->details->uri = gnome_vfs_uri_new (uri);

	if (view->details->directory_list != NULL)
		gnome_vfs_directory_list_destroy (view->details->directory_list);
	view->details->current_position = GNOME_VFS_DIRECTORY_LIST_POSITION_NONE;


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
		 sort_rules, 				/* sort_rules */
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

/**
 * fm_directory_view_sort:
 * 
 * Reorder the items in this view.
 * @view: FMDirectoryView whose items will be reordered.
 * @sort_type: FMDirectoryViewSortType specifying what new order to use.
 * @reverse_sort: TRUE if items should be sorted in reverse of standard order.
 * 
 **/
void
fm_directory_view_sort (FMDirectoryView *view,
			FMDirectoryViewSortType sort_type,
			gboolean reverse_sort)
{
	GnomeVFSDirectorySortRule *rules;

#define ALLOC_RULES(n) alloca ((n) * sizeof (GnomeVFSDirectorySortRule))

	g_return_if_fail (view != NULL);
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	if (view->details->directory_list == NULL)
		return;

	switch (sort_type) {
	case FM_DIRECTORY_VIEW_SORT_BYNAME:
		rules = ALLOC_RULES (2);
		/* Note: This used to put directories first. I
		 * thought that was counterproductive and removed it,
		 * but I can imagine discussing this further.
		 * John Sullivan <sullivan@eazel.com>
		 */
		rules[0] = GNOME_VFS_DIRECTORY_SORT_BYNAME;
		rules[1] = GNOME_VFS_DIRECTORY_SORT_NONE;
		break;
	case FM_DIRECTORY_VIEW_SORT_BYSIZE:
		rules = ALLOC_RULES (4);
		rules[0] = GNOME_VFS_DIRECTORY_SORT_DIRECTORYFIRST;
		rules[1] = GNOME_VFS_DIRECTORY_SORT_BYSIZE;
		rules[2] = GNOME_VFS_DIRECTORY_SORT_BYNAME;
		rules[3] = GNOME_VFS_DIRECTORY_SORT_NONE;
		break;
	case FM_DIRECTORY_VIEW_SORT_BYTYPE:
		rules = ALLOC_RULES (4);
		rules[0] = GNOME_VFS_DIRECTORY_SORT_DIRECTORYFIRST;
		rules[1] = GNOME_VFS_DIRECTORY_SORT_BYMIMETYPE;
		rules[2] = GNOME_VFS_DIRECTORY_SORT_BYNAME;
		rules[3] = GNOME_VFS_DIRECTORY_SORT_NONE;
		break;
	case FM_DIRECTORY_VIEW_SORT_BYMTIME:
		rules = ALLOC_RULES (3);
		rules[0] = GNOME_VFS_DIRECTORY_SORT_BYMTIME;
		rules[1] = GNOME_VFS_DIRECTORY_SORT_BYNAME;
		rules[2] = GNOME_VFS_DIRECTORY_SORT_NONE;
		break;
	default:
		g_warning ("fm_directory_view_sort: Unknown sort mode %d\n",
			   sort_type);
		return;
	}

	gnome_vfs_directory_list_sort (view->details->directory_list, reverse_sort, rules);

	fm_directory_view_populate (view);

#undef ALLOC_RULES
}

/**
 * nautilus_file_date_as_string:
 * 
 * Get a user-displayable string representing a file modification date. 
 * The caller is responsible for g_free-ing this string.
 * @file_info: GnomeVFSFileInfo representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
gchar *
nautilus_file_date_as_string (GnomeVFSFileInfo *file_info)
{
	/* Note: There's also accessed time and changed time.
	 * Accessed time doesn't seem worth showing to the user.
	 * Changed time is only subtly different from modified time
	 * (changed time includes "metadata" changes like file permissions).
	 * We should not display both, but we might change our minds as to
	 * which one is better.
	 */

	/* Note that ctime is a funky function that returns a
	 * string that you're not supposed to free.
	 */
	return g_strdup (ctime (&file_info->mtime));
}

/**
 * nautilus_file_size_as_string:
 * 
 * Get a user-displayable string representing a file size. The caller
 * is responsible for g_free-ing this string.
 * @file_info: GnomeVFSFileInfo representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
gchar *
nautilus_file_size_as_string (GnomeVFSFileInfo *file_info)
{
	if (file_info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
	{
		return g_strdup(_("--"));
	}

	return gnome_vfs_file_size_to_string (file_info->size);
}

/**
 * nautilus_file_type_as_string:
 * 
 * Get a user-displayable string representing a file type. The caller
 * is responsible for g_free-ing this string.
 * @file_info: GnomeVFSFileInfo representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
gchar *
nautilus_file_type_as_string (GnomeVFSFileInfo *file_info)
{
	if (file_info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
	{
		/* Special-case this so it isn't "special/directory".
		 * Should this be "folder" instead?
		 */		
		return g_strdup(_("directory"));
	}

	return g_strdup (file_info->mime_type);
}
