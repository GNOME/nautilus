/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-icon-text-window.h - interface for window that lets user modify 
 			   displayed icon text.

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

#ifndef FM_ICON_TEXT_WINDOW_H
#define FM_ICON_TEXT_WINDOW_H

#include <gtk/gtkwindow.h>

/* Interface for creating the window. */
GtkWindow *fm_icon_text_window_get_or_create (void);

/* Cover for getting the attribute names preference or the default.
 * This is really not part of icon text window, but putting it here
 * prevents us from having to abstract what attribute names are legal.
 */
char *fm_get_text_attribute_names_preference_or_default (void);

#endif /* FM_ICON_TEXT_WINDOW_H */
