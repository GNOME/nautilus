/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* explorer-directory-view.c
 *
 * Copyright (C) 1999  Free Software Foundaton
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

#include <gnome.h>

#include "gnome-icon-container.h"
#include "gtkflist.h"

#include "explorer-debug.h"
#include "explorer-directory-view.h"
#include "explorer-icon-manager.h"


enum {
	OPEN_FAILED,
	OPEN_DONE,
	LOAD_FAILED,
	LOAD_DONE,
	ACTIVATE_URI,
	LAST_SIGNAL
};

#define DISPLAY_TIMEOUT_INTERVAL 500


static GtkScrollFrameClass *parent_class = NULL;
static guint signals[LAST_SIGNAL] = { 0 };


static void
display_selection_info (ExplorerDirectoryView *view,
			GList *selection)
{
	GnomeVFSFileSize size;
	guint count;
	gchar *count_string, *size_string, *msg;
	GList *p;

	count = 0;
	size = 0;
	for (p = selection; p != NULL; p = p->next) {
		GnomeVFSFileInfo *info;

		info = p->data;
		count++;
		size += info->size;
	}

	if (count == 0) {
		gnome_appbar_set_status (view->app_bar, "");
		return;
	}

	/* FIXME: The following should probably go into a separate module, as
           we might have to do the same thing in other places as well.  Also,
	   I am not sure this will be OK for all the languages.  */

	if (size < (GnomeVFSFileSize) 1e3) {
		if (size == 1)
			size_string = g_strdup (_("1 byte"));
		else
			size_string = g_strdup_printf (_("%u bytes"),
						       (guint) size);
	} else {
		gdouble displayed_size;

		if (size < (GnomeVFSFileSize) 1e6) {
			displayed_size = (gdouble) size / 1.0e3;
			size_string = g_strdup_printf (_("%.1fK"),
						       displayed_size);
		} else if (size < (GnomeVFSFileSize) 1e9) {
			displayed_size = (gdouble) size / 1.0e6;
			size_string = g_strdup_printf (_("%.1fM"),
						       displayed_size);
		} else {
			displayed_size = (gdouble) size / 1.0e9;
			size_string = g_strdup_printf (_("%.1fG"),
						       displayed_size);
		}
	}

	if (count == 1)
		count_string = g_strdup (_("1 file."));
	else
		count_string = g_strdup_printf (_("%d files."), count);

	msg = g_strdup_printf (_("%s selected in %s"),
			       size_string, count_string);
	gnome_appbar_set_status (view->app_bar, msg);

	g_free (count_string);
	g_free (size_string);
	g_free (msg);
}


/* GnomeIconContainer handling.  */

static gboolean
mode_uses_icon_container (ExplorerDirectoryViewMode mode)
{
	return (mode == EXPLORER_DIRECTORY_VIEW_MODE_ICONS
		|| mode == EXPLORER_DIRECTORY_VIEW_MODE_SMALLICONS);
}

static gboolean
view_has_icon_container (ExplorerDirectoryView *view)
{
	return mode_uses_icon_container (view->mode);
}

static void
icon_container_selection_changed_cb (GnomeIconContainer *container,
				     gpointer data)
{
	ExplorerDirectoryView *view;
	GList *selection;

	view = EXPLORER_DIRECTORY_VIEW (data);

	selection = gnome_icon_container_get_selection (container);
	display_selection_info (view, selection);
	g_list_free (selection);
}

static void
icon_container_activate_cb (GnomeIconContainer *icon_container,
			    const gchar *name,
			    gpointer icon_data,
			    gpointer data)
{
	ExplorerDirectoryView *directory_view;
	GnomeVFSURI *new_uri;
	GnomeVFSFileInfo *info;

	info = (GnomeVFSFileInfo *) icon_data;
	directory_view = EXPLORER_DIRECTORY_VIEW (data);

	new_uri = gnome_vfs_uri_append_path (directory_view->uri, name);
	gtk_signal_emit (GTK_OBJECT (directory_view),
			 signals[ACTIVATE_URI], new_uri, info->mime_type);
	gnome_vfs_uri_unref (new_uri);
}

static GnomeIconContainer *
get_icon_container (ExplorerDirectoryView *view)
{
	GtkBin *bin;

	g_return_val_if_fail (view_has_icon_container (view), NULL);

	bin = GTK_BIN (view);

	if (bin->child == NULL)
		return NULL;	/* Avoid GTK+ complaints.  */
	else
		return GNOME_ICON_CONTAINER (bin->child);
}

static void
add_to_icon_container (ExplorerDirectoryView *view,
		       ExplorerIconManager *icon_manager,
		       GnomeIconContainer *icon_container,
		       GnomeVFSFileInfo *info,
		       gboolean with_layout)
{
	GdkImlibImage *image;

	image = explorer_icon_manager_get_icon_for_info (icon_manager, info);

	if (! with_layout || view->icon_layout == NULL) {
		gnome_icon_container_add_imlib_auto (icon_container,
						     image,
						     info->name,
						     info);
	} else {
		gboolean result;

		result = gnome_icon_container_add_imlib_with_layout
			(icon_container, image, info->name, info,
			 view->icon_layout);
		if (! result)
			view->icons_not_in_layout = g_list_prepend
				(view->icons_not_in_layout, info);
	}
}

static void
load_icon_container (ExplorerDirectoryView *view,
		     GnomeIconContainer *icon_container)
{
	gnome_icon_container_clear (icon_container);

	if (view->directory_list != NULL) {
		GnomeVFSDirectoryListPosition *position;
		ExplorerIconManager *icon_manager;

		icon_manager = explorer_application_get_icon_manager
			(view->application);

		position = gnome_vfs_directory_list_get_first_position
			(view->directory_list);

		while (position != view->current_position) {
			GnomeVFSFileInfo *info;

			info = gnome_vfs_directory_list_get
				(view->directory_list, position);
			add_to_icon_container (view, icon_manager,
					       icon_container, info, TRUE);

			position = gnome_vfs_directory_list_position_next
				(position);
		}
	}

}

static GnomeIconContainer *
create_icon_container (ExplorerDirectoryView *view)
{
	GnomeIconContainer *icon_container;

	icon_container = GNOME_ICON_CONTAINER (gnome_icon_container_new ());
	GTK_WIDGET_SET_FLAGS (icon_container, GTK_CAN_FOCUS);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "activate",
			    GTK_SIGNAL_FUNC (icon_container_activate_cb),
			    view);
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "selection_changed",
			    GTK_SIGNAL_FUNC (icon_container_selection_changed_cb),
			    view);

	gtk_container_add (GTK_CONTAINER (view), GTK_WIDGET (icon_container));

	gtk_widget_show (GTK_WIDGET (icon_container));
	load_icon_container (view, icon_container);

	return icon_container;
}

static void
setup_icon_container (ExplorerDirectoryView *view,
		      ExplorerDirectoryViewMode mode)
{
	GnomeIconContainer *icon_container;

	g_return_if_fail (mode_uses_icon_container (mode));

	if (! view_has_icon_container (view)) {
		GtkWidget *child;

		child = GTK_BIN (view)->child;
		if (child != NULL)
			gtk_widget_destroy (GTK_BIN (view)->child);
		icon_container = create_icon_container (view);
	} else {
		icon_container = get_icon_container (view);
	}

	if (mode == EXPLORER_DIRECTORY_VIEW_MODE_ICONS)
		gnome_icon_container_set_icon_mode
			(icon_container, GNOME_ICON_CONTAINER_NORMAL_ICONS);
	else
		gnome_icon_container_set_icon_mode
			(icon_container, GNOME_ICON_CONTAINER_SMALL_ICONS);
}


/* GtkFList handling.  */

static gboolean
mode_uses_flist (ExplorerDirectoryViewMode mode)
{
	return (mode == EXPLORER_DIRECTORY_VIEW_MODE_DETAILED
		|| mode == EXPLORER_DIRECTORY_VIEW_MODE_CUSTOM);
}

static gboolean
view_has_flist (ExplorerDirectoryView *view)
{
	return mode_uses_flist (view->mode);
}

static void
flist_selection_changed_cb (GtkFList *flist,
			    gpointer data)
{
	ExplorerDirectoryView *view;
	GList *selection;

	view = EXPLORER_DIRECTORY_VIEW (data);

	selection = gtk_flist_get_selection (flist);
	display_selection_info (view, selection);
	g_list_free (selection);
}

static void
flist_activate_cb (GtkFList *flist,
		   gpointer entry_data,
		   gpointer data)
{
	ExplorerDirectoryView *directory_view;
	GnomeVFSURI *new_uri;
	GnomeVFSFileInfo *info;

	info = (GnomeVFSFileInfo *) entry_data;
	directory_view = EXPLORER_DIRECTORY_VIEW (data);

	new_uri = gnome_vfs_uri_append_path (directory_view->uri, info->name);
	gtk_signal_emit (GTK_OBJECT (directory_view),
			 signals[ACTIVATE_URI], new_uri, info->mime_type);
	gnome_vfs_uri_unref (new_uri);
}

static GtkFList *
get_flist (ExplorerDirectoryView *view)
{
	GtkBin *bin;

	g_return_val_if_fail (view_has_flist (view), NULL);

	bin = GTK_BIN (view);

	if (bin->child == NULL)
		return NULL;	/* Avoid GTK+ complaints.  */
	else
		return GTK_FLIST (bin->child);
}

static void
add_to_flist (ExplorerIconManager *icon_manager,
	      GtkFList *flist,
	      GnomeVFSFileInfo *info)
{
	GtkCList *clist;
	gchar *text[2];

	text[0] = info->name;
	text[1] = NULL;

	clist = GTK_CLIST (flist);
	gtk_clist_append (clist, text);
	gtk_clist_set_row_data (clist, clist->rows - 1, info);
}

static GtkFList *
create_flist (ExplorerDirectoryView *view)
{
	GtkFList *flist;
	gchar *titles[] = {
		"Name",
		NULL
	};

	flist = GTK_FLIST (gtk_flist_new_with_titles (2, titles));
	gtk_clist_set_column_width (GTK_CLIST (flist), 0, 150); /* FIXME */
	GTK_WIDGET_SET_FLAGS (flist, GTK_CAN_FOCUS);

	gtk_signal_connect (GTK_OBJECT (flist),
			    "activate",
			    GTK_SIGNAL_FUNC (flist_activate_cb),
			    view);
	gtk_signal_connect (GTK_OBJECT (flist),
			    "selection_changed",
			    GTK_SIGNAL_FUNC (flist_selection_changed_cb),
			    view);

	gtk_container_add (GTK_CONTAINER (view), GTK_WIDGET (flist));

	gtk_widget_show (GTK_WIDGET (flist));

	if (view->directory_list != NULL) {
		GnomeVFSDirectoryListPosition *position;
		ExplorerIconManager *icon_manager;

		icon_manager = explorer_application_get_icon_manager
			(view->application);

		position = gnome_vfs_directory_list_get_first_position
			(view->directory_list);

		gtk_clist_freeze (GTK_CLIST (flist));

		while (position != view->current_position) {
			GnomeVFSFileInfo *info;

			info = gnome_vfs_directory_list_get
				(view->directory_list, position);
			add_to_flist (icon_manager, flist, info);

			position = gnome_vfs_directory_list_position_next
				(position);
		}

		gtk_clist_thaw (GTK_CLIST (flist));
	}

	return flist;
}

static void
setup_flist (ExplorerDirectoryView *view,
		      ExplorerDirectoryViewMode mode)
{
	GtkFList *flist;

	g_return_if_fail (mode_uses_flist (mode));

	if (! view_has_flist (view)) {
		GtkWidget *child;

		child = GTK_BIN (view)->child;
		if (child != NULL)
			gtk_widget_destroy (GTK_BIN (view)->child);
		flist = create_flist (view);
	}
}


/* Signals.  */

static void
real_open_failed (ExplorerDirectoryView *directory_view,
		  GnomeVFSResult result)
{
	g_return_if_fail (directory_view != NULL);
	g_return_if_fail (EXPLORER_IS_DIRECTORY_VIEW (directory_view));
}

static void
real_open_done (ExplorerDirectoryView *directory_view)
{
	g_return_if_fail (directory_view != NULL);
	g_return_if_fail (EXPLORER_IS_DIRECTORY_VIEW (directory_view));
}

static void
real_load_failed (ExplorerDirectoryView *directory_view,
		  GnomeVFSResult result)
{
	g_return_if_fail (directory_view != NULL);
	g_return_if_fail (EXPLORER_IS_DIRECTORY_VIEW (directory_view));
}

static void
real_load_done (ExplorerDirectoryView *directory_view)
{
	g_return_if_fail (directory_view != NULL);
	g_return_if_fail (EXPLORER_IS_DIRECTORY_VIEW (directory_view));
}

static void
real_activate_uri (ExplorerDirectoryView *directory_view,
		   const GnomeVFSURI *uri,
		   const gchar *mime_type)
{
	g_return_if_fail (directory_view != NULL);
	g_return_if_fail (EXPLORER_IS_DIRECTORY_VIEW (directory_view));
	g_return_if_fail (uri != NULL);
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	ExplorerDirectoryView *view;

	EXPLORER_DEBUG (("Entering function."));

	view = EXPLORER_DIRECTORY_VIEW (object);

	if (view->directory_list != NULL)
		gnome_vfs_directory_list_destroy (view->directory_list);

	if (view->uri != NULL)
		gnome_vfs_uri_unref (view->uri);

	if (view->vfs_async_handle != NULL) {
		EXPLORER_DEBUG (("Cancelling VFS operation."));
		gnome_vfs_async_cancel (view->vfs_async_handle);
	}

	if (view->display_timeout_id != 0)
		gtk_timeout_remove (view->display_timeout_id);

	if (view->icons_not_in_layout != NULL)
		g_list_free (view->icons_not_in_layout);

	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (ExplorerDirectoryViewClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);

	parent_class = gtk_type_class (gtk_scroll_frame_get_type ());

	signals[OPEN_FAILED] = 
		gtk_signal_new ("open_failed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ExplorerDirectoryViewClass,
						   open_failed),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 0,
				GTK_TYPE_INT);
	signals[OPEN_DONE] = 
		gtk_signal_new ("open_done",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ExplorerDirectoryViewClass,
						   open_done),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	signals[LOAD_FAILED] = 
		gtk_signal_new ("load_failed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ExplorerDirectoryViewClass,
						   load_failed),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 0,
				GTK_TYPE_INT);
	signals[LOAD_DONE] = 
		gtk_signal_new ("load_done",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ExplorerDirectoryViewClass,
						   load_done),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	signals[ACTIVATE_URI] = 
		gtk_signal_new ("activate_uri",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ExplorerDirectoryViewClass,
						   load_done),
				gtk_marshal_NONE__POINTER_POINTER,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_POINTER,
				GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	object_class->destroy = destroy;

	class->open_failed = real_open_failed;
	class->open_done = real_open_done;
	class->load_failed = real_load_failed;
	class->load_done = real_load_done;
	class->activate_uri = real_activate_uri;
}

static void
init (ExplorerDirectoryView *directory_view)
{
	GtkScrollFrame *scroll_frame;

	directory_view->application = NULL;
	directory_view->app_bar = NULL;

	directory_view->mode = EXPLORER_DIRECTORY_VIEW_MODE_NONE;

	directory_view->uri = NULL;
	directory_view->vfs_async_handle = NULL;
	directory_view->directory_list = NULL;

	directory_view->current_position = GNOME_VFS_DIRECTORY_LIST_POSITION_NONE;
	directory_view->entries_to_display = 0;

	directory_view->display_timeout_id = 0;

	directory_view->icon_layout = NULL;
	directory_view->icons_not_in_layout = NULL;

	scroll_frame = GTK_SCROLL_FRAME (directory_view);

	gtk_scroll_frame_set_hadjustment (scroll_frame, NULL);
	gtk_scroll_frame_set_vadjustment (scroll_frame, NULL);
	gtk_scroll_frame_set_policy (scroll_frame,
				     GTK_POLICY_AUTOMATIC,
				     GTK_POLICY_AUTOMATIC);
	gtk_scroll_frame_set_shadow_type (scroll_frame, GTK_SHADOW_IN);
}


/* Utility functions.  */

static void
stop_load (ExplorerDirectoryView *view)
{
	if (view->vfs_async_handle != NULL) {
		gnome_vfs_async_cancel (view->vfs_async_handle);
		view->vfs_async_handle = NULL;
	}

	if (view->display_timeout_id != 0) {
		gtk_timeout_remove (view->display_timeout_id);
		view->display_timeout_id = 0;
	}

	view->current_position = GNOME_VFS_DIRECTORY_LIST_POSITION_NONE;
	view->entries_to_display = 0;
}


static void
display_pending_entries (ExplorerDirectoryView *view)
{
	ExplorerIconManager *icon_manager;
	GnomeIconContainer *icon_container;
	GtkFList *flist;
	guint i;

	EXPLORER_DEBUG (("Adding %d entries.", view->entries_to_display));

	icon_manager = explorer_application_get_icon_manager (view->application);

	if (view_has_icon_container (view)) {
		icon_container = get_icon_container (view);
		flist = NULL;
	} else {
		icon_container = NULL;
		flist = get_flist (view);
		gtk_clist_freeze (GTK_CLIST (flist));
	}

	for (i = 0; i < view->entries_to_display; i++) {
		GnomeVFSFileInfo *info;

		info = gnome_vfs_directory_list_get (view->directory_list,
						     view->current_position);

		if (icon_container != NULL)
			add_to_icon_container (view, icon_manager,
					       icon_container, info, TRUE);
		else
			add_to_flist (icon_manager, flist, info);

		view->current_position = gnome_vfs_directory_list_position_next
						       (view->current_position);
	}

	if (flist != NULL)
		gtk_clist_thaw (GTK_CLIST (flist));

	view->entries_to_display = 0;

	EXPLORER_DEBUG (("Done."));
}

static void
display_icons_not_in_layout (ExplorerDirectoryView *view)
{
	ExplorerIconManager *icon_manager;
	GnomeIconContainer *icon_container;
	GList *p;

	if (view->icons_not_in_layout == NULL)
		return;

	EXPLORER_DEBUG (("Adding entries not in layout."));

	icon_manager = explorer_application_get_icon_manager (view->application);

	icon_container = get_icon_container (view);
	g_return_if_fail (icon_container != NULL);

	/* FIXME: This will block if there are many files.  */

	for (p = view->icons_not_in_layout; p != NULL; p = p->next) {
		GnomeVFSFileInfo *info;

		info = p->data;
		add_to_icon_container (view, icon_manager,
				       icon_container, info, FALSE);
		EXPLORER_DEBUG (("Adding `%s'", info->name));
	}

	EXPLORER_DEBUG (("Done with entries not in layout."));

	g_list_free (view->icons_not_in_layout);
	view->icons_not_in_layout = NULL;
}

static gboolean
display_timeout_cb (gpointer data)
{
	ExplorerDirectoryView *view;

	EXPLORER_DEBUG (("Entering function"));

	view = EXPLORER_DIRECTORY_VIEW (data);

	display_pending_entries (view);

	EXPLORER_DEBUG (("Done"));

	return TRUE;
}


/* Set up the base URI for Drag & Drop operations.  */
static void
setup_base_uri (ExplorerDirectoryView *view)
{
	GnomeIconContainer *icon_container;
	gchar *txt_uri;

	txt_uri = gnome_vfs_uri_to_string (view->uri, 0);
	if (txt_uri == NULL)
		return;

	icon_container = get_icon_container (view);
	if (icon_container != NULL)
		gnome_icon_container_set_base_uri (icon_container, txt_uri);

	g_free (txt_uri);
}

static void
directory_load_cb (GnomeVFSAsyncHandle *handle,
		   GnomeVFSResult result,
		   GnomeVFSDirectoryList *list,
		   guint entries_read,
		   gpointer callback_data)
{
	ExplorerDirectoryView *view;

	EXPLORER_DEBUG (("Entering function, %d entries read: %s",
			 entries_read, gnome_vfs_result_to_string (result)));

	view = EXPLORER_DIRECTORY_VIEW (callback_data);

	if (view->directory_list == NULL) {
		if (result == GNOME_VFS_OK || result == GNOME_VFS_ERROR_EOF) {
			gtk_signal_emit (GTK_OBJECT (view), signals[OPEN_DONE]);

			setup_base_uri (view);

			view->directory_list = list;

			/* FIXME just to make sure.  But these should be
			   already set somewhere else.  */
			view->current_position
				= GNOME_VFS_DIRECTORY_LIST_POSITION_NONE;
			view->entries_to_display = 0;

			if (result != GNOME_VFS_ERROR_EOF)
				view->display_timeout_id
					= gtk_timeout_add
						(DISPLAY_TIMEOUT_INTERVAL,
						 display_timeout_cb,
						 view);
		} else if (entries_read == 0) {
			gtk_signal_emit (GTK_OBJECT (view),
					 signals[OPEN_FAILED]);
		}
	}

	if (view->current_position == GNOME_VFS_DIRECTORY_LIST_POSITION_NONE)
		view->current_position
			= gnome_vfs_directory_list_get_position (list);

	view->entries_to_display += entries_read;

	if (result == GNOME_VFS_ERROR_EOF) {
		display_pending_entries (view);
		display_icons_not_in_layout (view);
		stop_load (view);
		gtk_signal_emit (GTK_OBJECT (view), signals[LOAD_DONE]);
	} else if (result != GNOME_VFS_OK) {
		stop_load (view);
		gtk_signal_emit (GTK_OBJECT (view), signals[LOAD_FAILED],
				 result);
		return;
	}
}


gboolean
explorer_directory_view_is_valid_mode (ExplorerDirectoryViewMode mode)
{
	switch (mode) {
	case EXPLORER_DIRECTORY_VIEW_MODE_ICONS:
	case EXPLORER_DIRECTORY_VIEW_MODE_SMALLICONS:
	case EXPLORER_DIRECTORY_VIEW_MODE_DETAILED:
	case EXPLORER_DIRECTORY_VIEW_MODE_CUSTOM:
		return TRUE;
	case EXPLORER_DIRECTORY_VIEW_MODE_NONE:
	default:
		return FALSE;
	}
}

GtkType
explorer_directory_view_get_type (void)
{
	static GtkType directory_view_type = 0;

	if (directory_view_type == 0) {
		static GtkTypeInfo directory_view_info = {
			"ExplorerDirectoryView",
			sizeof (ExplorerDirectoryView),
			sizeof (ExplorerDirectoryViewClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL
		};

		directory_view_type
			= gtk_type_unique (gtk_scroll_frame_get_type (),
					   &directory_view_info);
	}

	return directory_view_type;
}

GtkWidget *
explorer_directory_view_new (ExplorerApplication *application,
			     GnomeAppBar *app_bar,
			     ExplorerDirectoryViewMode mode)
{
	ExplorerDirectoryView *new;

	g_return_val_if_fail (application != NULL, NULL);
	g_return_val_if_fail (explorer_directory_view_is_valid_mode (mode), NULL);

	new = gtk_type_new (explorer_directory_view_get_type ());

	new->application = application;
	new->app_bar = app_bar;

	explorer_directory_view_set_mode (new, mode);

	return GTK_WIDGET (new);
}

ExplorerDirectoryViewMode
explorer_directory_view_get_mode (ExplorerDirectoryView *view)
{
	g_return_val_if_fail (view != NULL, EXPLORER_DIRECTORY_VIEW_MODE_ICONS);
	g_return_val_if_fail (EXPLORER_IS_DIRECTORY_VIEW (view),
			      EXPLORER_DIRECTORY_VIEW_MODE_ICONS);

	return view->mode;
}

void
explorer_directory_view_set_mode (ExplorerDirectoryView *view,
				  ExplorerDirectoryViewMode mode)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (EXPLORER_IS_DIRECTORY_VIEW (view));

	if (view->mode == mode)
		return;

	switch (mode) {
	case EXPLORER_DIRECTORY_VIEW_MODE_ICONS:
	case EXPLORER_DIRECTORY_VIEW_MODE_SMALLICONS:
		setup_icon_container (view, mode);
		break;
	case EXPLORER_DIRECTORY_VIEW_MODE_DETAILED:
	case EXPLORER_DIRECTORY_VIEW_MODE_CUSTOM:
		setup_flist (view, mode);
		break;
	case EXPLORER_DIRECTORY_VIEW_MODE_NONE:
		break;
	}

	view->mode = mode;
}


void
explorer_directory_view_load_uri (ExplorerDirectoryView *view,
				  const GnomeVFSURI *uri)
{
	static GnomeVFSDirectorySortRule sort_rules[] = {
		GNOME_VFS_DIRECTORY_SORT_DIRECTORYFIRST,
		GNOME_VFS_DIRECTORY_SORT_BYNAME,
		GNOME_VFS_DIRECTORY_SORT_NONE
	};			/* FIXME */
	GnomeVFSResult result;

	g_return_if_fail (view != NULL);
	g_return_if_fail (EXPLORER_IS_DIRECTORY_VIEW (view));
	g_return_if_fail (uri != NULL);

	explorer_directory_view_stop (view);

	if (view->uri != NULL)
		gnome_vfs_uri_unref (view->uri);
	view->uri = gnome_vfs_uri_dup (uri);

	result = gnome_vfs_async_load_directory_uri
		(&view->vfs_async_handle, 		/* handle */
		 view->uri,				/* uri */
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
		 1,			 		/* items_per_notification */
		 directory_load_cb,	 		/* callback */
		 view);		 			/* callback_data */

	if (result != GNOME_VFS_OK)
		gtk_signal_emit (GTK_OBJECT (view), signals[OPEN_FAILED],
				 result);
}

void
explorer_directory_view_stop (ExplorerDirectoryView *view)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (EXPLORER_IS_DIRECTORY_VIEW (view));

	if (view->vfs_async_handle == NULL)
		return;

	stop_load (view);
}


/* WARNING WARNING WARNING

   These two functions actually do completely different things, although they
   have similiar name.  (Actually, maybe I should change these names: FIXME.)

   The `get' function retrieves the current *actual* layout from the icon
   container.  The `set' function, instead, specifies the layout that will be
   used when adding new files to the view.  */

GnomeIconContainerLayout *
explorer_directory_view_get_icon_layout (ExplorerDirectoryView *view)
{
	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (EXPLORER_IS_DIRECTORY_VIEW (view), NULL);

	if (mode_uses_icon_container (view->mode)) {
		GnomeIconContainer *icon_container;

		icon_container = get_icon_container (view);
		return gnome_icon_container_get_layout (icon_container);
	}

	return NULL;
}

void
explorer_directory_view_set_icon_layout (ExplorerDirectoryView *view,
					 const GnomeIconContainerLayout *layout)
{
	g_return_if_fail (view != NULL);

	view->icon_layout = layout;
}

void
explorer_directory_view_line_up_icons (ExplorerDirectoryView *view)
{
	GnomeIconContainer *container;

	g_return_if_fail (view != NULL);

	container = get_icon_container (view);
	if (container == NULL)
		return;

	gnome_icon_container_line_up (container);
}


void
explorer_directory_view_sort (ExplorerDirectoryView *view,
			      ExplorerDirectoryViewSortType sort_type)
{
	GnomeVFSDirectorySortRule *rules;
	GnomeIconContainer *icon_container;

#define ALLOC_RULES(n) alloca ((n) * sizeof (GnomeVFSDirectorySortRule))

	g_return_if_fail (view != NULL);
	g_return_if_fail (EXPLORER_IS_DIRECTORY_VIEW (view));

	if (view->directory_list == NULL)
		return;

	switch (sort_type) {
	case EXPLORER_DIRECTORY_VIEW_SORT_BYNAME:
		rules = ALLOC_RULES (3);
		rules[0] = GNOME_VFS_DIRECTORY_SORT_DIRECTORYFIRST;
		rules[1] = GNOME_VFS_DIRECTORY_SORT_BYNAME;
		rules[2] = GNOME_VFS_DIRECTORY_SORT_NONE;
		break;
	case EXPLORER_DIRECTORY_VIEW_SORT_BYSIZE:
		rules = ALLOC_RULES (4);
		rules[0] = GNOME_VFS_DIRECTORY_SORT_DIRECTORYFIRST;
		rules[1] = GNOME_VFS_DIRECTORY_SORT_BYSIZE;
		rules[2] = GNOME_VFS_DIRECTORY_SORT_BYNAME;
		rules[3] = GNOME_VFS_DIRECTORY_SORT_NONE;
		break;
	case EXPLORER_DIRECTORY_VIEW_SORT_BYTYPE:
		rules = ALLOC_RULES (4);
		rules[0] = GNOME_VFS_DIRECTORY_SORT_DIRECTORYFIRST;
		rules[1] = GNOME_VFS_DIRECTORY_SORT_BYMIMETYPE;
		rules[2] = GNOME_VFS_DIRECTORY_SORT_BYNAME;
		rules[3] = GNOME_VFS_DIRECTORY_SORT_NONE;
		break;
	default:
		g_warning ("explorer_directory_view_sort: Unknown sort mode %d\n",
			   sort_type);
		return;
	}

	EXPLORER_DEBUG (("Sorting."));
	gnome_vfs_directory_list_sort (view->directory_list, FALSE, rules);

	/* This will make sure icons are re-laid out according to the new
           order.  */
	if (view->icon_layout != NULL)
		view->icon_layout = NULL;

	/* FIXME FIXME FIXME */
	icon_container = get_icon_container (view);
	if (icon_container != NULL)
		load_icon_container (view, icon_container);

#undef ALLOC_RULES
}
