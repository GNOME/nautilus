/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-desktop-window.h

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

   Authors: Darin Adler <darin@eazel.com>
*/

#ifndef NAUTILUS_DESKTOP_WINDOW_H
#define NAUTILUS_DESKTOP_WINDOW_H

#include "nautilus-window.h"
#include "nautilus-application.h"

#define NAUTILUS_TYPE_DESKTOP_WINDOW            (nautilus_desktop_window_get_type())
#define NAUTILUS_DESKTOP_WINDOW(object)         (GTK_CHECK_CAST ((object), NAUTILUS_TYPE_DESKTOP_WINDOW, NautilusDesktopWindow))
#define NAUTILUS_DESKTOP_WINDOW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_DESKTOP_WINDOW, NautilusDesktopWindowClass))
#define NAUTILUS_IS_DESKTOP_WINDOW(object)      (GTK_CHECK_TYPE ((object), NAUTILUS_TYPE_DESKTOP_WINDOW))
#define NAUTILUS_IS_DESKTOP_WINDOW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_DESKTOP_WINDOW))

typedef struct NautilusDesktopWindowDetails NautilusDesktopWindowDetails;

typedef struct {
	NautilusWindow parent_spot;
	NautilusDesktopWindowDetails *details;
} NautilusDesktopWindow;

typedef struct {
	NautilusWindowClass parent_spot;
} NautilusDesktopWindowClass;

GtkType                nautilus_desktop_window_get_type (void);
NautilusDesktopWindow *nautilus_desktop_window_new      (NautilusApplication *application);

#endif /* NAUTILUS_DESKTOP_WINDOW_H */
