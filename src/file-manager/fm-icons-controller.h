/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-icons-controller.h: Abstract interface for icons in the
   FMIconsView (currently named GnomeIconContainer).
 
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

#ifndef FM_ICONS_CONTROLLER_H
#define FM_ICONS_CONTROLLER_H

#include <libnautilus/nautilus-icons-controller.h>
#include "fm-directory-view-icons.h"

typedef struct _FMIconsController FMIconsController;
typedef struct _FMIconsControllerClass FMIconsControllerClass;

#define FM_TYPE_ICONS_CONTROLLER \
	(fm_icons_controller_get_type ())
#define FM_ICONS_CONTROLLER(obj) \
	(GTK_CHECK_CAST ((obj), FM_TYPE_ICONS_CONTROLLER, FMIconsController))
#define FM_ICONS_CONTROLLER_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), FM_TYPE_ICONS_CONTROLLER, FMIconsControllerClass))
#define FM_IS_ICONS_CONTROLLER(obj) \
	(GTK_CHECK_TYPE ((obj), FM_TYPE_ICONS_CONTROLLER))
#define FM_IS_ICONS_CONTROLLER_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), FM_TYPE_ICONS_CONTROLLER))

/* Basic GtkObject requirements. */
GtkType            fm_icons_controller_get_type (void);
FMIconsController *fm_icons_controller_new      (FMDirectoryViewIcons *icons);

struct _FMIconsController
{
	NautilusIconsController abstract_controller;
	FMDirectoryViewIcons *icons;
};

struct _FMIconsControllerClass
{
	NautilusIconsControllerClass parent_class;
};

#endif /* FM_ICONS_CONTROLLER_H */
