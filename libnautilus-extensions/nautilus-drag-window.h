/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 2001 Eazel, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: John Harper <jsh@eazel.com>
 *
 */

#ifndef NAUTILUS_DRAG_WINDOW_H
#define NAUTILUS_DRAG_WINDOW_H

#include <gtk/gtkwindow.h>

/* Call this function before WINDOW has been realized. It will hook
 * into the window so that it automatically supports the correct focus
 * policy when dragging objects from within the window. (This policy is
 * *not* to focus or raise the window when the activating click is used
 * to drag something)
 */

void nautilus_drag_window_register (GtkWindow *window);

#endif /* NAUTILUS_DRAG_WINDOW_H */
