/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-icon-container.h - the container widget for file manager icons

   Copyright (C) 2002 Sun Microsystems, Inc.

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

   Author: Michael Meeks <michael@ximian.com>
*/

#ifndef FM_ICON_CONTAINER_H
#define FM_ICON_CONTAINER_H

#include <libnautilus-private/nautilus-icon-container.h>
#include "fm-icon-view.h"

typedef struct FMIconContainer FMIconContainer;
typedef struct FMIconContainerClass FMIconContainerClass;

#define FM_TYPE_ICON_CONTAINER			(fm_icon_container_get_type ())
#define FM_ICON_CONTAINER(obj)			(GTK_CHECK_CAST ((obj), FM_TYPE_ICON_CONTAINER, FMIconContainer))
#define FM_ICON_CONTAINER_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), FM_TYPE_ICON_CONTAINER, FMIconContainerClass))
#define FM_IS_ICON_CONTAINER(obj)			(GTK_CHECK_TYPE ((obj), FM_TYPE_ICON_CONTAINER))
#define FM_IS_ICON_CONTAINER_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((klass), FM_TYPE_ICON_CONTAINER))

typedef struct FMIconContainerDetails FMIconContainerDetails;

struct FMIconContainer {
	NautilusIconContainer parent;

	FMIconView *view;
	gboolean    sort_for_desktop;
};

struct FMIconContainerClass {
	NautilusIconContainerClass parent_class;
};

GType                  fm_icon_container_get_type         (void);
NautilusIconContainer *fm_icon_container_construct        (FMIconContainer *icon_container,
							   FMIconView      *view);
NautilusIconContainer *fm_icon_container_new              (FMIconView      *view);
void                   fm_icon_container_set_sort_desktop (FMIconContainer *container,
							   gboolean         desktop);

#endif /* FM_ICON_CONTAINER_H */
