/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-icons-controller.h: Abstract interface for icons in the
   NautilusIconsView (currently named GnomeIconContainer).
 
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
  
   Author: Darin Adler <darin@eazel.com>
*/

#ifndef NAUTILUS_ICONS_CONTROLLER_H
#define NAUTILUS_ICONS_CONTROLLER_H

#include <gtk/gtkobject.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "nautilus-icon-factory.h"

/* NautilusIconsController is an abstract class that describes the
   interface that a NautilusIconsView (currently named GnomeIconContainer)
   uses to manipulate the icons.
*/

typedef struct NautilusIconsController NautilusIconsController;
typedef struct NautilusIconsControllerClass NautilusIconsControllerClass;

#define NAUTILUS_TYPE_ICONS_CONTROLLER \
	(nautilus_icons_controller_get_type ())
#define NAUTILUS_ICONS_CONTROLLER(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_ICONS_CONTROLLER, NautilusIconsController))
#define NAUTILUS_ICONS_CONTROLLER_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ICONS_CONTROLLER, NautilusIconsControllerClass))
#define NAUTILUS_IS_ICONS_CONTROLLER(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_ICONS_CONTROLLER))
#define NAUTILUS_IS_ICONS_CONTROLLER_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_ICONS_CONTROLLER))

typedef struct NautilusControllerIconDummy NautilusControllerIcon;

#define NAUTILUS_CONTROLLER_ICON(icon) \
	((NautilusControllerIcon *) (icon))

/* Basic GtkObject requirements. */
GtkType               nautilus_icons_controller_get_type          (void);

/* Icon operations. */
NautilusScalableIcon *nautilus_icons_controller_get_icon_image    (NautilusIconsController  *controller,
								   NautilusControllerIcon   *icon,
								   GList                   **emblem_images);
char *                nautilus_icons_controller_get_icon_property (NautilusIconsController  *controller,
								   NautilusControllerIcon   *icon,
								   const char               *property_name);
char *                nautilus_icons_controller_get_icon_text     (NautilusIconsController  *controller,
								   NautilusControllerIcon   *icon);
char *                nautilus_icons_controller_get_icon_uri      (NautilusIconsController  *controller,
								   NautilusControllerIcon   *icon);

struct NautilusIconsController
{
	GtkObject object;
};

struct NautilusIconsControllerClass
{
	GtkObjectClass parent_class;

	NautilusScalableIcon * (* get_icon_image)    (NautilusIconsController  *controller,
						      NautilusControllerIcon   *icon,
						      GList                   **emblem_images);
	char *  	       (* get_icon_property) (NautilusIconsController  *controller,
						      NautilusControllerIcon   *icon,
						      const char               *property_name);
	char *                 (* get_icon_text)     (NautilusIconsController  *controller,
						      NautilusControllerIcon   *icon);
	char *                 (* get_icon_uri)      (NautilusIconsController  *controller,
						      NautilusControllerIcon   *icon);
};

#endif /* NAUTILUS_ICONS_CONTROLLER_H */
