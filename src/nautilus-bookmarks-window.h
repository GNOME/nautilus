/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-bookmarks-window.h - interface for bookmark-editing window.

   Copyright (C) 1999, 2000 Eazel, Inc.

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

#ifndef NAUTILUS_BOOKMARKS_WINDOW_H
#define NAUTILUS_BOOKMARKS_WINDOW_H

#include <gtk/gtkwindow.h>
#include "nautilus-bookmark-list.h"

GtkWindow *create_bookmarks_window                 (NautilusBookmarkList *bookmarks,
						    GtkObject            *undo_manager_source);
void       nautilus_bookmarks_window_save_geometry (GtkWindow            *window);

#endif /* NAUTILUS_BOOKMARKS_WINDOW_H */
