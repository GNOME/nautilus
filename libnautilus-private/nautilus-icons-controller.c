/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-icons-controller.c: Abstract interface for getting information
   about icons in NautilusIconsView (GnomeIconContainer).
 
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

#include <config.h>
#include "nautilus-icons-controller.h"

#include "nautilus-gtk-macros.h"

static void nautilus_icons_controller_initialize_class (NautilusIconsControllerClass *klass);
static void nautilus_icons_controller_initialize (NautilusIconsController *controller);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusIconsController, nautilus_icons_controller, GTK_TYPE_OBJECT)

NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (nautilus_icons_controller, get_icon_image)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (nautilus_icons_controller, get_icon_property)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (nautilus_icons_controller, get_icon_text)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (nautilus_icons_controller, get_icon_uri)

static void
nautilus_icons_controller_initialize_class (NautilusIconsControllerClass *klass)
{
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, nautilus_icons_controller, get_icon_image);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, nautilus_icons_controller, get_icon_property);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, nautilus_icons_controller, get_icon_text);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, nautilus_icons_controller, get_icon_uri);
}

static void
nautilus_icons_controller_initialize (NautilusIconsController *controller)
{
}

NautilusScalableIcon *
nautilus_icons_controller_get_icon_image (NautilusIconsController *controller,
					  NautilusControllerIcon *icon,
					  GList **emblem_images)
{
	return (* NAUTILUS_ICONS_CONTROLLER_CLASS (controller->object.klass)->get_icon_image)
		(controller, icon, emblem_images);
}

char *
nautilus_icons_controller_get_icon_property (NautilusIconsController *controller,
					     NautilusControllerIcon *icon,
					     const char *property_name)
{
	return (* NAUTILUS_ICONS_CONTROLLER_CLASS (controller->object.klass)->get_icon_property)
		(controller, icon, property_name);
}

char *
nautilus_icons_controller_get_icon_text (NautilusIconsController *controller,
					 NautilusControllerIcon *icon)
{
	return (* NAUTILUS_ICONS_CONTROLLER_CLASS (controller->object.klass)->get_icon_text)
		(controller, icon);
}

char *
nautilus_icons_controller_get_icon_uri (NautilusIconsController *controller,
					NautilusControllerIcon *icon)
{
	return (* NAUTILUS_ICONS_CONTROLLER_CLASS (controller->object.klass)->get_icon_uri)
		(controller, icon);
}
