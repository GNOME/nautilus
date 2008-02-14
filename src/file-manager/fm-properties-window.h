/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-properties-window.h - interface for window that lets user modify 
                            icon properties

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

   Authors: Darin Adler <darin@bentspoon.com>
*/

#ifndef FM_PROPERTIES_WINDOW_H
#define FM_PROPERTIES_WINDOW_H

#include <gtk/gtkwindow.h>
#include <libnautilus-private/nautilus-file.h>

typedef struct FMPropertiesWindow FMPropertiesWindow;

#define FM_TYPE_PROPERTIES_WINDOW \
	(fm_properties_window_get_type ())
#define FM_PROPERTIES_WINDOW(obj) \
	(GTK_CHECK_CAST ((obj), FM_TYPE_PROPERTIES_WINDOW, FMPropertiesWindow))
#define FM_PROPERTIES_WINDOW_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), FM_TYPE_PROPERTIES_WINDOW, FMPropertiesWindowClass))
#define FM_IS_PROPERTIES_WINDOW(obj) \
	(GTK_CHECK_TYPE ((obj), FM_TYPE_PROPERTIES_WINDOW))
#define FM_IS_PROPERTIES_WINDOW_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), FM_TYPE_PROPERTIES_WINDOW))

typedef struct FMPropertiesWindowDetails FMPropertiesWindowDetails;

struct FMPropertiesWindow {
	GtkWindow window;
	FMPropertiesWindowDetails *details;	
};

struct FMPropertiesWindowClass {
	GtkWindowClass parent_class;
	
	/* Keybinding signals */
	void (* close)    (FMPropertiesWindow *window);
};

typedef struct FMPropertiesWindowClass FMPropertiesWindowClass;

GType   fm_properties_window_get_type   (void);

void 	fm_properties_window_present 	(GList *files,
					 GtkWidget *parent_widget);

#endif /* FM_PROPERTIES_WINDOW_H */
