/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-annotation-window.h - interface for window that lets user add and edit
   file annotations.          

   Copyright (C) 2001 Eazel, Inc.

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

   Authors: Andy Hertzfeld <andy@eazel.com>
*/

#ifndef FM_ANNOTATION_WINDOW_H
#define FM_ANNOTATION_WINDOW_H

#include "fm-directory-view.h"

#include <gtk/gtkwindow.h>
#include <libgnomeui/gnome-dialog.h>
#include <libnautilus-extensions/nautilus-file.h>

typedef struct FMAnnotationWindow FMAnnotationWindow;

#define FM_TYPE_ANNOTATION_WINDOW \
	(fm_annotation_window_get_type ())
#define FM_ANNOTATION_WINDOW(obj) \
	(GTK_CHECK_CAST ((obj), FM_TYPE_ANNOTATION_WINDOW, FMAnnotationWindow))
#define FM_ANNOTATION_WINDOW_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), FM_TYPE_ANNOTATION_WINDOW, FMAnnotationWindowClass))
#define FM_IS_ANNOTATION_WINDOW(obj) \
	(GTK_CHECK_TYPE ((obj), FM_TYPE_ANNOTATION_WINDOW))
#define FM_IS_ANNOTATION_WINDOW_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), FM_TYPE_ANNOTATION_WINDOW))

typedef struct FMAnnotationWindowDetails FMAnnotationWindowDetails;

struct FMAnnotationWindow {
	GnomeDialog dialog;
	FMAnnotationWindowDetails *details;	
};

struct FMAnnotationWindowClass {
	GnomeDialogClass parent_class;
};

typedef struct FMAnnotationWindowClass FMAnnotationWindowClass;

GtkType fm_annotation_window_get_type   (void);
void 	fm_annotation_window_present 	(NautilusFile    *file,
					 FMDirectoryView *directory_view);

#endif /* FM_ANNOTATION_WINDOW_H */
