/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-background-canvas-group.h: Class used internally by
   NautilusBackground to add a background to a canvas.

   Copyright (C) 2000 Eazel, Inc.
   
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Authors: Darin Adler <darin@eazel.com>
*/

#ifndef NAUTILUS_BACKGROUND_CANVAS_GROUP_H
#define NAUTILUS_BACKGROUND_CANVAS_GROUP_H

#include "nautilus-background.h"
#include <libgnomeui/gnome-canvas.h>

/* A NautilusBackgroundCanvasGroup is used internally by NautilusBackground to change
   the color of a canvas. The reason we have to change the class of a canvas group is
   that the cleanest way to hook into the code that erases the canvas is to be the
   root canvas group. But the canvas class creates the root object and doesn't allow
   it to be destroyed, so we change the class of the root object in place.

   A future version of GnomeCanvas may allow a nicer way of hooking in to the code
   that draws the background, and then we can get rid of this class.

   This class is private to NautilusBackground.
*/

typedef GnomeCanvasGroup NautilusBackgroundCanvasGroup;
typedef GnomeCanvasGroupClass NautilusBackgroundCanvasGroupClass;

#define NAUTILUS_TYPE_BACKGROUND_CANVAS_GROUP \
	(nautilus_background_canvas_group_get_type ())
#define NAUTILUS_BACKGROUND_CANVAS_GROUP(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_BACKGROUND_CANVAS_GROUP, NautilusBackgroundCanvasGroup))
#define NAUTILUS_BACKGROUND_CANVAS_GROUP_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_BACKGROUND_CANVAS_GROUP, NautilusBackgroundCanvasGroupClass))
#define NAUTILUS_IS_BACKGROUND_CANVAS_GROUP(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_BACKGROUND_CANVAS_GROUP))
#define NAUTILUS_IS_BACKGROUND_CANVAS_GROUP_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_BACKGROUND_CANVAS_GROUP))

GtkType             nautilus_background_canvas_group_get_type       (void);

#endif /* NAUTILUS_BACKGROUND_CANVAS_GROUP_H */
