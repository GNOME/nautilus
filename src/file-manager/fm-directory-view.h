/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* fm-directory-view.h
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
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
#ifndef __FM_DIRECTORY_VIEW_H__
#define __FM_DIRECTORY_VIEW_H__

#include <libgnomevfs/gnome-vfs.h>

#include <libnautilus/libnautilus.h>
#include <libnautilus/gnome-icon-container.h>
#include <libnautilus/gtkscrollframe.h>


enum _FMDirectoryViewMode {
	FM_DIRECTORY_VIEW_MODE_NONE, /* Internal */
	FM_DIRECTORY_VIEW_MODE_ICONS,
	FM_DIRECTORY_VIEW_MODE_SMALLICONS,
	FM_DIRECTORY_VIEW_MODE_DETAILED,
	FM_DIRECTORY_VIEW_MODE_CUSTOM
};
typedef enum   _FMDirectoryViewMode  FMDirectoryViewMode;

enum _FMDirectoryViewSortType {
	FM_DIRECTORY_VIEW_SORT_BYNAME,
	FM_DIRECTORY_VIEW_SORT_BYSIZE,
	FM_DIRECTORY_VIEW_SORT_BYTYPE
};
typedef enum _FMDirectoryViewSortType FMDirectoryViewSortType;


typedef struct _FMDirectoryView      FMDirectoryView;
typedef struct _FMDirectoryViewClass FMDirectoryViewClass;

#define FM_TYPE_DIRECTORY_VIEW			(fm_directory_view_get_type ())
#define FM_DIRECTORY_VIEW(obj)			(GTK_CHECK_CAST ((obj), FM_TYPE_DIRECTORY_VIEW, FMDirectoryView))
#define FM_DIRECTORY_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), FM_TYPE_DIRECTORY_VIEW, FMDirectoryViewClass))
#define FM_IS_DIRECTORY_VIEW(obj)			(GTK_CHECK_TYPE ((obj), FM_TYPE_DIRECTORY_VIEW))
#define FM_IS_DIRECTORY_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), FM_TYPE_DIRECTORY_VIEW))

struct _FMDirectoryView {
	NautilusContentViewFrame parent;

	FMDirectoryViewMode mode;

	GtkWidget *scroll_frame;

	GnomeVFSDirectoryList *directory_list;
	GnomeVFSDirectoryListPosition current_position;
	guint entries_to_display;

	guint display_timeout_id;

	GnomeVFSAsyncHandle *vfs_async_handle;
	GnomeVFSURI *uri;

	const GnomeIconContainerLayout *icon_layout;
	GList *icons_not_in_layout;

	/* Idle ID for displaying information about the current selection at
           idle time.  */
	gint display_selection_idle_id;
};

struct _FMDirectoryViewClass {
	NautilusContentViewFrameClass parent_class;
};


gboolean   fm_directory_view_is_valid_mode
					    (FMDirectoryViewMode mode);

GtkType    fm_directory_view_get_type (void);
GtkWidget *fm_directory_view_new      (void);
void	   fm_directory_view_set_mode (FMDirectoryView *view,
				       FMDirectoryViewMode mode);
FMDirectoryViewMode
	   fm_directory_view_get_mode (FMDirectoryView *view);
void       fm_directory_view_load_uri (FMDirectoryView *view,
				       const char *uri);
void	   fm_directory_view_stop      (FMDirectoryView *view);

GnomeIconContainerLayout *
	   fm_directory_view_get_icon_layout
				            (FMDirectoryView *view);
void	   fm_directory_view_set_icon_layout
					    (FMDirectoryView *view,
					     const GnomeIconContainerLayout
					     *icon_layout);

void	   fm_directory_view_line_up_icons
					    (FMDirectoryView *view);

void	   fm_directory_view_sort     (FMDirectoryView *view,
				       FMDirectoryViewSortType sort_type);

#endif /* __FM_DIRECTORY_VIEW_H__ */
