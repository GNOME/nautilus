/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* explorer-directory-view.h
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
#ifndef __EXPLORER_DIRECTORY_VIEW_H__
#define __EXPLORER_DIRECTORY_VIEW_H__

#include <libgnomevfs/gnome-vfs.h>

#include "gnome-icon-container.h"
#include "gtkscrollframe.h"


enum _ExplorerDirectoryViewMode {
	EXPLORER_DIRECTORY_VIEW_MODE_NONE, /* Internal */
	EXPLORER_DIRECTORY_VIEW_MODE_ICONS,
	EXPLORER_DIRECTORY_VIEW_MODE_SMALLICONS,
	EXPLORER_DIRECTORY_VIEW_MODE_DETAILED,
	EXPLORER_DIRECTORY_VIEW_MODE_CUSTOM
};
typedef enum   _ExplorerDirectoryViewMode  ExplorerDirectoryViewMode;

enum _ExplorerDirectoryViewSortType {
	EXPLORER_DIRECTORY_VIEW_SORT_BYNAME,
	EXPLORER_DIRECTORY_VIEW_SORT_BYSIZE,
	EXPLORER_DIRECTORY_VIEW_SORT_BYTYPE
};
typedef enum _ExplorerDirectoryViewSortType ExplorerDirectoryViewSortType;


typedef struct _ExplorerDirectoryView      ExplorerDirectoryView;
typedef struct _ExplorerDirectoryViewClass ExplorerDirectoryViewClass;

#include "explorer-application.h"


#define EXPLORER_TYPE_DIRECTORY_VIEW			(explorer_directory_view_get_type ())
#define EXPLORER_DIRECTORY_VIEW(obj)			(GTK_CHECK_CAST ((obj), EXPLORER_TYPE_DIRECTORY_VIEW, ExplorerDirectoryView))
#define EXPLORER_DIRECTORY_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), EXPLORER_TYPE_DIRECTORY_VIEW, ExplorerDirectoryViewClass))
#define EXPLORER_IS_DIRECTORY_VIEW(obj)			(GTK_CHECK_TYPE ((obj), EXPLORER_TYPE_DIRECTORY_VIEW))
#define EXPLORER_IS_DIRECTORY_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), EXPLORER_TYPE_DIRECTORY_VIEW))

struct _ExplorerDirectoryView {
	GtkScrollFrame scroll_frame;

	ExplorerApplication *application;
	GnomeAppBar *app_bar;

	ExplorerDirectoryViewMode mode;

	GnomeVFSDirectoryList *directory_list;
	GnomeVFSDirectoryListPosition current_position;
	guint entries_to_display;

	guint display_timeout_id;

	GnomeVFSAsyncHandle *vfs_async_handle;
	GnomeVFSURI *uri;

	const GnomeIconContainerLayout *icon_layout;
	GList *icons_not_in_layout;
};

struct _ExplorerDirectoryViewClass {
	GtkScrollFrameClass parent_class;

	/* Signals go here */
	void (*open_failed)	(ExplorerDirectoryView *directory_view,
				 GnomeVFSResult result);
	void (*open_done)	(ExplorerDirectoryView *directory_view);
	void (*load_failed)	(ExplorerDirectoryView *directory_view,
				 GnomeVFSResult result);
	void (*load_done)	(ExplorerDirectoryView *directory_view);
	void (*activate_uri)	(ExplorerDirectoryView *directory_view,
				 const GnomeVFSURI *uri,
				 const gchar *mime_type);
};


gboolean   explorer_directory_view_is_valid_mode
					    (ExplorerDirectoryViewMode mode);

GtkType    explorer_directory_view_get_type (void);
GtkWidget *explorer_directory_view_new      (ExplorerApplication *application,
					     GnomeAppBar *app_bar,
					     ExplorerDirectoryViewMode mode);
void	   explorer_directory_view_set_mode (ExplorerDirectoryView *view,
					     ExplorerDirectoryViewMode mode);
ExplorerDirectoryViewMode
	   explorer_directory_view_get_mode (ExplorerDirectoryView *view);
void       explorer_directory_view_load_uri (ExplorerDirectoryView *view,
					     const GnomeVFSURI *uri);
void	   explorer_directory_view_stop	    (ExplorerDirectoryView *view);

GnomeIconContainerLayout *
	   explorer_directory_view_get_icon_layout
				            (ExplorerDirectoryView *view);
void	   explorer_directory_view_set_icon_layout
					    (ExplorerDirectoryView *view,
					     const GnomeIconContainerLayout
					            *icon_layout);

void	   explorer_directory_view_line_up_icons
					    (ExplorerDirectoryView *view);

void	   explorer_directory_view_sort     (ExplorerDirectoryView *view,
					     ExplorerDirectoryViewSortType sort_type);

#endif /* __EXPLORER_DIRECTORY_VIEW_H__ */
