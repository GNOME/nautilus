/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-empty-view.h - interface for empty view of directory.

   Copyright (C) 2006 Free Software Foundation, Inc.
   
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

   Authors: Christian Neumair <chris@gnome-de.org>
*/

#ifndef FM_EMPTY_VIEW_H
#define FM_EMPTY_VIEW_H

#include "fm-directory-view.h"

#define FM_TYPE_EMPTY_VIEW		(fm_empty_view_get_type ())
#define FM_EMPTY_VIEW(obj)		(GTK_CHECK_CAST ((obj), FM_TYPE_EMPTY_VIEW, FMEmptyView))
#define FM_EMPTY_VIEW_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), FM_TYPE_EMPTY_VIEW, FMEmptyViewClass))
#define FM_IS_EMPTY_VIEW(obj)		(GTK_CHECK_TYPE ((obj), FM_TYPE_EMPTY_VIEW))
#define FM_IS_EMPTY_VIEW_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), FM_TYPE_EMPTY_VIEW))

#define FM_EMPTY_VIEW_ID "OAFIID:Nautilus_File_Manager_Empty_View"

typedef struct FMEmptyViewDetails FMEmptyViewDetails;

typedef struct {
	FMDirectoryView parent_instance;
	FMEmptyViewDetails *details;
} FMEmptyView;

typedef struct {
	FMDirectoryViewClass parent_class;
} FMEmptyViewClass;

GType fm_empty_view_get_type (void);
void  fm_empty_view_register (void);

#endif /* FM_EMPTY_VIEW_H */
