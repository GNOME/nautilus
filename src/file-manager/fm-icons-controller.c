/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   fm-icons-controller.c: Controller that puts NautilusFile together
   with the GnomeIconContainer.
 
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
#include "fm-icons-controller.h"

#include <libnautilus/nautilus-gtk-macros.h>
#include <libnautilus/nautilus-icon-factory.h>

static void       fm_icons_controller_initialize_class (FMIconsControllerClass  *klass);
static void       fm_icons_controller_initialize       (FMIconsController       *controller);
static GdkPixbuf *fm_icons_controller_get_icon_image   (NautilusIconsController *controller,
							NautilusControllerIcon  *icon);
static char *     fm_icons_controller_get_icon_name    (NautilusIconsController *controller,
							NautilusControllerIcon  *icon);
static char *     fm_icons_controller_get_icon_uri     (NautilusIconsController *controller,
							NautilusControllerIcon  *icon);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMIconsController, fm_icons_controller, NAUTILUS_TYPE_ICONS_CONTROLLER)

static void
fm_icons_controller_initialize_class (FMIconsControllerClass *klass)
{
	NautilusIconsControllerClass *abstract_class;

	abstract_class = NAUTILUS_ICONS_CONTROLLER_CLASS (klass);

	abstract_class->get_icon_image = fm_icons_controller_get_icon_image;
	abstract_class->get_icon_name = fm_icons_controller_get_icon_name;
	abstract_class->get_icon_uri = fm_icons_controller_get_icon_uri;
}

static void
fm_icons_controller_initialize (FMIconsController *controller)
{
}

FMIconsController *
fm_icons_controller_new (FMDirectoryViewIcons *icons)
{
	FMIconsController *controller;

	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (icons), NULL);

	controller = FM_ICONS_CONTROLLER(gtk_object_new (FM_TYPE_ICONS_CONTROLLER, NULL));
	controller->icons = icons;

	return controller;
}

static GdkPixbuf *
fm_icons_controller_get_icon_image (NautilusIconsController *controller,
				    NautilusControllerIcon *icon)
{
	/* Get the appropriate image for the file. For the moment,
	 * we always use the standard size of icons.
	 */

	NautilusScalableIcon *scalable_icon;
	GdkPixbuf *pixbuf;

	scalable_icon = nautilus_icon_factory_get_icon_for_file
		(NAUTILUS_FILE (icon));
	pixbuf = nautilus_icon_factory_get_pixbuf_for_icon
		(scalable_icon,
		 NAUTILUS_ICON_SIZE_STANDARD);
	nautilus_scalable_icon_unref (scalable_icon);

	return pixbuf;
}

static char *
fm_icons_controller_get_icon_name (NautilusIconsController *controller,
				   NautilusControllerIcon *icon)
{
	return nautilus_file_get_name (NAUTILUS_FILE (icon));
}

static char *
fm_icons_controller_get_icon_uri (NautilusIconsController *controller,
				  NautilusControllerIcon *icon)
{
	return nautilus_file_get_uri (NAUTILUS_FILE (icon));
}
