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

#include <gnome.h>

#include <libnautilus/libnautilus.h>
#include <libnautilus/gnome-icon-container.h>
#include <libnautilus/gtkflist.h>

#include "fm-directory-view.h"
#include "fm-icon-cache.h"
#include "fm-public-api.h"

#define FM_DEBUG(x) g_message x

#define WITH_LAYOUT TRUE
#define DISPLAY_TIMEOUT_INTERVAL 500
#define ENTRIES_PER_CB 1

static GtkScrolledWindowClass *parent_class = NULL;

/* FIXME this no longer has any reason to be global,
   given fm_get_current_icon_cache()
*/
static FMIconCache *icm = NULL;

void
display_selection_info (FMDirectoryView *view,
			GList *selection)
{
	GnomeVFSFileSize size;
	guint count;
	gchar *size_string, *msg;
	GList *p;
	Nautilus_StatusRequestInfo sri;

	count = 0;
	size = 0;
	for (p = selection; p != NULL; p = p->next) {
		GnomeVFSFileInfo *info;

		info = p->data;
		count++;
		size += info->size;
	}

	if (count == 0) {
	        memset(&sri, 0, sizeof(sri));
	        sri.status_string = "";
	        nautilus_view_frame_request_status_change(NAUTILUS_VIEW_FRAME(view->view_frame), &sri);
		return;
	}

	/* FIXME: The following should probably go into a separate module, as
           we might have to do the same thing in other places as well.  Also,
	   I am not sure this will be OK for all the languages.  */

	/* FIXME: gnome-vfs has an internal routine that also does this
	   (gnome_vfs_size_to_string())
	*/
	
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

	msg = g_strdup_printf (_("%d %s selected -- %s"),
		    count, (count==1)?_("file"):_("files"), size_string);
	memset(&sri, 0, sizeof(sri));
	sri.status_string = msg;
	nautilus_view_frame_request_status_change(NAUTILUS_VIEW_FRAME(view->view_frame), &sri);
        g_free (msg);

	g_free (size_string);
}


/* GnomeIconContainer handling.  */

static gboolean
mode_uses_icon_container (FMDirectoryViewMode mode)
{
	return (mode == FM_DIRECTORY_VIEW_MODE_ICONS
		|| mode == FM_DIRECTORY_VIEW_MODE_SMALLICONS);
}

static gboolean
view_has_icon_container (FMDirectoryView *view)
{
	return mode_uses_icon_container (view->mode);
}

GnomeIconContainer *
get_icon_container (FMDirectoryView *view)
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
add_to_icon_container (FMDirectoryView *view,
		       FMIconCache *icon_manager,
		       GnomeIconContainer *icon_container,
		       GnomeVFSFileInfo *info,
		       gboolean with_layout)
{
	GdkPixbuf *image;

	g_return_if_fail(info);

	image = fm_icon_cache_get_icon (icon_manager, info);

	if (! with_layout || view->icon_layout == NULL) {
		gnome_icon_container_add_pixbuf_auto (icon_container,
						     image,
						     info->name,
						     info);
	} else {
		gboolean result;

		result = gnome_icon_container_add_pixbuf_with_layout
			(icon_container, image, info->name, info,
			 view->icon_layout);
		if (! result)
			view->icons_not_in_layout = g_list_prepend
				(view->icons_not_in_layout, info);
	}
}

void
load_icon_container (FMDirectoryView *view,
		     GnomeIconContainer *icon_container)
{
	gnome_icon_container_clear (icon_container);

	if (view->directory_list != NULL) {
		GnomeVFSDirectoryListPosition *position;

		if (!icm)
			icm = fm_get_current_icon_cache();

		position = gnome_vfs_directory_list_get_first_position
			(view->directory_list);

		while (position != view->current_position) {
			GnomeVFSFileInfo *info;

			info = gnome_vfs_directory_list_get
				(view->directory_list, position);

			g_return_if_fail(info);
			add_to_icon_container (view, icm,
					       icon_container, info, WITH_LAYOUT);

			position = gnome_vfs_directory_list_position_next
				(position);
		}
	}

}


/* GtkFList handling.  */

static gboolean
mode_uses_flist (FMDirectoryViewMode mode)
{
	return (mode == FM_DIRECTORY_VIEW_MODE_DETAILED
		|| mode == FM_DIRECTORY_VIEW_MODE_CUSTOM);
}

static gboolean
view_has_flist (FMDirectoryView *view)
{
	return mode_uses_flist (view->mode);
}

GtkFList *
get_flist (FMDirectoryView *view)
{
	GtkBin *bin;

	g_return_val_if_fail (view_has_flist (view), NULL);

	bin = GTK_BIN (view);

	if (bin->child == NULL)
		return NULL;	/* Avoid GTK+ complaints.  */
	else
		return GTK_FLIST (bin->child);
}

void
add_to_flist (FMIconCache *icon_manager,
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

static void
notify_location_change_cb (NautilusViewFrame *view_frame, Nautilus_NavigationInfo *nav_context, FMDirectoryView *directory_view)
{
	puts("******* XXX notify_location_change");
	g_message("Directory view is loading URL %s", nav_context->requested_uri);
	fm_directory_view_load_uri(directory_view, nav_context->requested_uri);
}

static void
stop_location_change_cb (NautilusViewFrame *view_frame, FMDirectoryView *directory_view)
{
	printf("*********** B %x\n", (unsigned) directory_view);

	g_message("Directory view is stopping");
	fm_directory_view_stop(directory_view);
}

/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	FMDirectoryView *view;

	FM_DEBUG (("Entering function."));

	view = FM_DIRECTORY_VIEW (object);

	if (view->directory_list != NULL)
		gnome_vfs_directory_list_destroy (view->directory_list);

	if (view->uri != NULL)
		gnome_vfs_uri_unref (view->uri);

	if (view->vfs_async_handle != NULL) {
		FM_DEBUG (("Cancelling VFS operation."));
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
class_init (FMDirectoryViewClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);

	parent_class = gtk_type_class (gtk_type_parent(object_class->type));
	object_class->destroy = destroy;
}

static void
init (FMDirectoryView *directory_view)
{
	directory_view->mode = FM_DIRECTORY_VIEW_MODE_NONE;

	directory_view->uri = NULL;
	directory_view->vfs_async_handle = NULL;
	directory_view->directory_list = NULL;

	directory_view->current_position = GNOME_VFS_DIRECTORY_LIST_POSITION_NONE;
	directory_view->entries_to_display = 0;

	directory_view->display_timeout_id = 0;

	directory_view->icon_layout = NULL;
	directory_view->icons_not_in_layout = NULL;

	directory_view->display_selection_idle_id = 0;

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

	directory_view->view_frame = 
		NAUTILUS_CONTENT_VIEW_FRAME (gtk_widget_new 
					     (nautilus_content_view_frame_get_type(), 
					      NULL));
	
	printf("*********** A %x\n", (unsigned) directory_view);

	gtk_signal_connect (GTK_OBJECT(directory_view->view_frame), 
			    "stop_location_change",
			    GTK_SIGNAL_FUNC (stop_location_change_cb),
			    directory_view);

	gtk_signal_connect (GTK_OBJECT(directory_view->view_frame), 
			    "notify_location_change",
			    GTK_SIGNAL_FUNC (notify_location_change_cb), 
			    directory_view);

	gtk_widget_show(GTK_WIDGET(directory_view));

	gtk_container_add(GTK_CONTAINER(directory_view->view_frame),
			  GTK_WIDGET(directory_view));
}


/* Utility functions.  */

static void
stop_load (FMDirectoryView *view, gboolean error)
{
	Nautilus_ProgressRequestInfo pri;

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
	view->directory_list = NULL;

	memset(&pri, 0, sizeof(pri));
	pri.amount = 100.0;
	pri.type = error ? Nautilus_PROGRESS_DONE_ERROR : Nautilus_PROGRESS_DONE_OK;
	
	nautilus_view_frame_request_progress_change(NAUTILUS_VIEW_FRAME(view->view_frame),
						     &pri);
}


static void
display_pending_entries (FMDirectoryView *view)
{
	FMIconCache *icon_manager;
	GnomeIconContainer *icon_container;
	GtkFList *flist;
	guint i;

	FM_DEBUG (("Adding %d entries.", view->entries_to_display));

	if(!icm)
	       icm = fm_get_current_icon_cache();
	icon_manager = icm;

	if (view_has_icon_container (view)) {
		icon_container = get_icon_container (view);
		flist = NULL;
	} else {
		icon_container = NULL;
		flist = get_flist (view);
		gtk_clist_freeze (GTK_CLIST (flist));
	}

	for (i = 0; i < view->entries_to_display
		     && view->current_position != GNOME_VFS_DIRECTORY_LIST_POSITION_NONE; i++) {
		GnomeVFSFileInfo *info;

		info = gnome_vfs_directory_list_get (view->directory_list,
						     view->current_position);

		if (icon_container != NULL)
			add_to_icon_container (view, icon_manager,
					       icon_container, info, WITH_LAYOUT);
		else
			add_to_flist (icon_manager, flist, info);

		view->current_position = gnome_vfs_directory_list_position_next
						       (view->current_position);
	}

	if (i != view->entries_to_display)
		g_warning("BROKEN! we thought we had %d items, actually had %d",
			  view->entries_to_display, i);

	if (flist != NULL)
		gtk_clist_thaw (GTK_CLIST (flist));

	view->entries_to_display = 0;

	FM_DEBUG (("Done."));
}

static void
display_icons_not_in_layout (FMDirectoryView *view)
{
	FMIconCache *icon_manager;
	GnomeIconContainer *icon_container;
	GList *p;

	if (view->icons_not_in_layout == NULL)
		return;

	FM_DEBUG (("Adding entries not in layout."));

	if (!icm)
		icm = fm_get_current_icon_cache();
	icon_manager = icm;

	icon_container = get_icon_container (view);
	g_return_if_fail (icon_container != NULL);

	/* FIXME: This will block if there are many files.  */

	for (p = view->icons_not_in_layout; p != NULL; p = p->next) {
		GnomeVFSFileInfo *info;

		info = p->data;
		add_to_icon_container (view, icon_manager,
				       icon_container, info, FALSE);
		FM_DEBUG (("Adding `%s'", info->name));
	}

	FM_DEBUG (("Done with entries not in layout."));

	g_list_free (view->icons_not_in_layout);
	view->icons_not_in_layout = NULL;
}

static gboolean
display_timeout_cb (gpointer data)
{
	FMDirectoryView *view;
	
	FM_DEBUG (("Entering function"));

	view = FM_DIRECTORY_VIEW (data);

	display_pending_entries (view);

	FM_DEBUG (("Done"));

	return TRUE;
}


/* Set up the base URI for Drag & Drop operations.  */
static void
setup_base_uri (FMDirectoryView *view)
{
	GnomeIconContainer *icon_container;
	gchar *txt_uri;

	txt_uri = gnome_vfs_uri_to_string (view->uri, 0);
	if (txt_uri == NULL)
		return;

	if (view_has_icon_container (view))
	{
		icon_container = get_icon_container (view);
		if (icon_container != NULL)
			gnome_icon_container_set_base_uri (icon_container, txt_uri);
	}

	g_free (txt_uri);
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

	if (view->directory_list == NULL) {
		if (result == GNOME_VFS_OK || result == GNOME_VFS_ERROR_EOF) {

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
			/*
			gtk_signal_emit (GTK_OBJECT (view),
					 signals[OPEN_FAILED]);
			*/
		}
	}

	if(!view->current_position && list)
		view->current_position
			= gnome_vfs_directory_list_get_position (list);

	view->entries_to_display += entries_read;

	if (result == GNOME_VFS_ERROR_EOF) {
		display_pending_entries (view);
		display_icons_not_in_layout (view);
		stop_load (view, FALSE);
		/* gtk_signal_emit (GTK_OBJECT (view), signals[LOAD_DONE]); */
	} else if (result != GNOME_VFS_OK) {
		stop_load (view, TRUE);
		/* gtk_signal_emit (GTK_OBJECT (view), signals[LOAD_FAILED],
		   result); */
		return;
	}
}


gboolean
fm_directory_view_is_valid_mode (FMDirectoryViewMode mode)
{
	switch (mode) {
	case FM_DIRECTORY_VIEW_MODE_ICONS:
	case FM_DIRECTORY_VIEW_MODE_SMALLICONS:
	case FM_DIRECTORY_VIEW_MODE_DETAILED:
	case FM_DIRECTORY_VIEW_MODE_CUSTOM:
		return TRUE;
	case FM_DIRECTORY_VIEW_MODE_NONE:
	default:
		return FALSE;
	}
}

GtkType
fm_directory_view_get_type (void)
{
	static GtkType directory_view_type = 0;

	if (directory_view_type == 0) {
		static GtkTypeInfo directory_view_info = {
			"FMDirectoryView",
			sizeof (FMDirectoryView),
			sizeof (FMDirectoryViewClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL
		};

		directory_view_type
			= gtk_type_unique (gtk_scrolled_window_get_type (),
					   &directory_view_info);
	}

	return directory_view_type;
}

GtkWidget *
fm_directory_view_new (void)
{
	return gtk_widget_new(fm_directory_view_get_type (), NULL);
}

FMDirectoryViewMode
fm_directory_view_get_mode (FMDirectoryView *view)
{
	g_return_val_if_fail (view != NULL, FM_DIRECTORY_VIEW_MODE_ICONS);
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view),
			      FM_DIRECTORY_VIEW_MODE_ICONS);

	return view->mode;
}

void
fm_directory_view_set_mode (FMDirectoryView *view,
			    FMDirectoryViewMode new_mode)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	if (view->mode == new_mode)
		return;

	view->mode = new_mode;
}


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

	if(view_has_icon_container(view))
	{
		gnome_icon_container_clear(get_icon_container(view));
	}
	if(view_has_flist(view))
		gtk_clist_clear(GTK_CLIST(get_flist(view)));

	if (view->uri != NULL)
		gnome_vfs_uri_unref (view->uri);
	view->uri = gnome_vfs_uri_new (uri);

	memset(&pri, 0, sizeof(pri));
	pri.type = Nautilus_PROGRESS_UNDERWAY;
	pri.amount = 0;
	nautilus_view_frame_request_progress_change(NAUTILUS_VIEW_FRAME(view->view_frame), &pri);

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
		 ENTRIES_PER_CB,			 /* items_per_notification */
		 directory_load_cb,	 		/* callback */
		 view);		 			/* callback_data */

	g_return_if_fail(result == GNOME_VFS_OK);
}

void
fm_directory_view_stop (FMDirectoryView *view)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	if (view->vfs_async_handle == NULL)
		return;

	display_pending_entries (view);
	display_icons_not_in_layout (view);
	stop_load (view, FALSE);
}


/* WARNING WARNING WARNING

   These two functions actually do completely different things, although they
   have similiar name.  (Actually, maybe I should change these names: FIXME.)

   The `get' function retrieves the current *actual* layout from the icon
   container.  The `set' function, instead, specifies the layout that will be
   used when adding new files to the view.  */

GnomeIconContainerLayout *
fm_directory_view_get_icon_layout (FMDirectoryView *view)
{
	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (view), NULL);

	if (mode_uses_icon_container (view->mode)) {
		GnomeIconContainer *icon_container;

		icon_container = get_icon_container (view);
		return gnome_icon_container_get_layout (icon_container);
	}

	return NULL;
}

void
fm_directory_view_set_icon_layout (FMDirectoryView *view,
				   const GnomeIconContainerLayout *layout)
{
	g_return_if_fail (view != NULL);

	view->icon_layout = layout;
}

void
fm_directory_view_line_up_icons (FMDirectoryView *view)
{
	GnomeIconContainer *container;

	g_return_if_fail (view != NULL);

	container = get_icon_container (view);
	if (container == NULL)
		return;

	gnome_icon_container_line_up (container);
}


void
fm_directory_view_sort (FMDirectoryView *view,
			FMDirectoryViewSortType sort_type)
{
	GnomeVFSDirectorySortRule *rules;
	GnomeIconContainer *icon_container;

#define ALLOC_RULES(n) alloca ((n) * sizeof (GnomeVFSDirectorySortRule))

	g_return_if_fail (view != NULL);
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (view));

	if (view->directory_list == NULL)
		return;

	switch (sort_type) {
	case FM_DIRECTORY_VIEW_SORT_BYNAME:
		rules = ALLOC_RULES (3);
		rules[0] = GNOME_VFS_DIRECTORY_SORT_DIRECTORYFIRST;
		rules[1] = GNOME_VFS_DIRECTORY_SORT_BYNAME;
		rules[2] = GNOME_VFS_DIRECTORY_SORT_NONE;
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
	default:
		g_warning ("fm_directory_view_sort: Unknown sort mode %d\n",
			   sort_type);
		return;
	}

	FM_DEBUG (("Sorting."));
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
