/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-scrolled-window.h - Subclass of GtkScrolledWindow that
				emits a "scroll_changed" signal.

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

   Authors: John Sullivan <sullivan@eazel.com>
*/

#ifndef NAUTILUS_SCROLLED_WINDOW_H
#define NAUTILUS_SCROLLED_WINDOW_H

#include <gtk/gtkscrolledwindow.h>

typedef struct NautilusScrolledWindow NautilusScrolledWindow;

#define NAUTILUS_TYPE_SCROLLED_WINDOW \
	(nautilus_scrolled_window_get_type ())
#define NAUTILUS_SCROLLED_WINDOW(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_SCROLLED_WINDOW, NautilusScrolledWindow))
#define NAUTILUS_SCROLLED_WINDOW_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SCROLLED_WINDOW, NautilusScrolledWindowClass))
#define NAUTILUS_IS_SCROLLED_WINDOW(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_SCROLLED_WINDOW))
#define NAUTILUS_IS_SCROLLED_WINDOW_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SCROLLED_WINDOW))

struct NautilusScrolledWindow {
	GtkScrolledWindow parent;
};

struct NautilusScrolledWindowClass {
	GtkScrolledWindowClass parent_class;

	/* Signals that clients can connect to. */

	void	(* scroll_changed) (NautilusScrolledWindow *window);
};

typedef struct NautilusScrolledWindowClass NautilusScrolledWindowClass;

GtkType nautilus_scrolled_window_get_type        (void);
void	nautilus_scrolled_window_set_hadjustment (NautilusScrolledWindow *scrolled_window,
						  GtkAdjustment		 *hadjustment);
void	nautilus_scrolled_window_set_vadjustment (NautilusScrolledWindow *scrolled_window,
						  GtkAdjustment		 *vadjustment);

#endif /* NAUTILUS_SCROLLED_WINDOW_H */
