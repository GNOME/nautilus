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

#ifndef __FM_DIRECTORY_VIEW_ICONS_H__
#define __FM_DIRECTORY_VIEW_ICONS_H__



typedef struct _FMDirectoryViewIcons      FMDirectoryViewIcons;
typedef struct _FMDirectoryViewIconsClass FMDirectoryViewIconsClass;

#define FM_TYPE_DIRECTORY_VIEW_ICONS			(fm_directory_view_icons_get_type ())
#define FM_DIRECTORY_VIEW_ICONS(obj)			(GTK_CHECK_CAST ((obj), FM_TYPE_DIRECTORY_VIEW_ICONS, FMDirectoryViewIcons))
#define FM_DIRECTORY_VIEW_ICONS_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), FM_TYPE_DIRECTORY_VIEW_ICONS, FMDirectoryViewIconsClass))
#define FM_IS_DIRECTORY_VIEW_ICONS(obj)			(GTK_CHECK_TYPE ((obj), FM_TYPE_DIRECTORY_VIEW_ICONS))
#define FM_IS_DIRECTORY_VIEW_ICONS_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), FM_TYPE_DIRECTORY_VIEW_ICONS))

struct _FMDirectoryViewIcons {
	FMDirectoryView parent;
};

struct _FMDirectoryViewIconsClass {
	FMDirectoryViewClass parent_class;
};


GtkType    fm_directory_view_icons_get_type (void);
GtkWidget *fm_directory_view_icons_new      (void);

GnomeIconContainerLayout *
	   fm_directory_view_icons_get_icon_layout
				            (FMDirectoryViewIcons *view);
void	   fm_directory_view_icons_set_icon_layout
					    (FMDirectoryViewIcons *view,
					     const GnomeIconContainerLayout
					     *icon_layout);
void	   fm_directory_view_icons_line_up_icons
					    (FMDirectoryViewIcons *view);


#endif /* __FM_DIRECTORY_VIEW_ICONS_H__ */
