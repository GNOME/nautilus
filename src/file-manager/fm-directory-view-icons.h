/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-directory-view-icons.h - interface for icon view of directory.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: John Sullivan <sullivan@eazel.com>
*/

#ifndef FM_DIRECTORY_VIEW_ICONS_H
#define FM_DIRECTORY_VIEW_ICONS_H

#include "fm-directory-view.h"
#include <libnautilus/gnome-icon-container.h>

typedef struct _FMDirectoryViewIcons      FMDirectoryViewIcons;
typedef struct _FMDirectoryViewIconsClass FMDirectoryViewIconsClass;

#define FM_TYPE_DIRECTORY_VIEW_ICONS			(fm_directory_view_icons_get_type ())
#define FM_DIRECTORY_VIEW_ICONS(obj)			(GTK_CHECK_CAST ((obj), FM_TYPE_DIRECTORY_VIEW_ICONS, FMDirectoryViewIcons))
#define FM_DIRECTORY_VIEW_ICONS_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), FM_TYPE_DIRECTORY_VIEW_ICONS, FMDirectoryViewIconsClass))
#define FM_IS_DIRECTORY_VIEW_ICONS(obj)			(GTK_CHECK_TYPE ((obj), FM_TYPE_DIRECTORY_VIEW_ICONS))
#define FM_IS_DIRECTORY_VIEW_ICONS_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), FM_TYPE_DIRECTORY_VIEW_ICONS))

typedef struct _FMDirectoryViewIconsDetails FMDirectoryViewIconsDetails;

struct _FMDirectoryViewIcons {
	FMDirectoryView parent;
	FMDirectoryViewIconsDetails *details;
};

struct _FMDirectoryViewIconsClass {
	FMDirectoryViewClass parent_class;
};

/* GtkObject support */
GtkType fm_directory_view_icons_get_type                           (void);

/* Functions for FMIconsController support. */
char *  fm_directory_view_icons_get_icon_text_attribute_names      (FMDirectoryViewIcons *view);
char *  fm_directory_view_icons_get_full_icon_text_attribute_names (FMDirectoryViewIcons *view);
void	fm_directory_view_icons_set_full_icon_text_attribute_names (FMDirectoryViewIcons *view,
							    	    char *new_names,
							    	    gboolean set_default);

/*
 * FIXME: None of the following are currently used. Remove them eventually if
 * we're not going to use them.
 */
void    fm_directory_view_icons_line_up_icons 		           (FMDirectoryViewIcons *view);

#endif /* FM_DIRECTORY_VIEW_ICONS_H */
