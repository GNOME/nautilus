/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-icon-view.h - interface for icon view of directory.

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

#ifndef FM_ICON_VIEW_H
#define FM_ICON_VIEW_H

#include "fm-directory-view.h"

typedef struct FMIconView FMIconView;
typedef struct FMIconViewClass FMIconViewClass;

#define FM_TYPE_ICON_VIEW			(fm_icon_view_get_type ())
#define FM_ICON_VIEW(obj)			(GTK_CHECK_CAST ((obj), FM_TYPE_ICON_VIEW, FMIconView))
#define FM_ICON_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), FM_TYPE_ICON_VIEW, FMIconViewClass))
#define FM_IS_ICON_VIEW(obj)			(GTK_CHECK_TYPE ((obj), FM_TYPE_ICON_VIEW))
#define FM_IS_ICON_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), FM_TYPE_ICON_VIEW))

typedef struct FMIconViewDetails FMIconViewDetails;

struct FMIconView {
	FMDirectoryView parent;
	FMIconViewDetails *details;
};

struct FMIconViewClass {
	FMDirectoryViewClass parent_class;
};

/* GtkObject support */
GtkType fm_icon_view_get_type (void);

#endif /* FM_ICON_VIEW_H */
