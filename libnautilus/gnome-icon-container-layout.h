/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-icon-layout.h

   Copyright (C) 1999 Free Software Foundation

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

   Author: Ettore Perazzoli <ettore@gnu.org>
*/

#ifndef _GNOME_ICON_CONTAINER_LAYOUT_H
#define _GNOME_ICON_CONTAINER_LAYOUT_H

#include <glib.h>

typedef struct _GnomeIconContainerLayout GnomeIconContainerLayout;

typedef void (* GnomeIconContainerLayoutForeachFunc)
				(const GnomeIconContainerLayout *layout,
				 const gchar *text,
				 gint x, gint y,
				 gpointer callback_data);

#include "gnome-icon-container.h"
#include "gnome-icon-container-private.h"


GnomeIconContainerLayout *
	gnome_icon_container_layout_new		 (void);
void	gnome_icon_container_layout_free 	 (GnomeIconContainerLayout *layout);
void	gnome_icon_container_layout_add		 (GnomeIconContainerLayout *layout,
						  const gchar *icon,
						  gint x,
						  gint y);
gboolean
	gnome_icon_container_layout_get_position (const GnomeIconContainerLayout *layout,
						  const gchar *icon,
						  gint *x_return,
						  gint *y_return);

void	gnome_icon_container_layout_foreach	 (GnomeIconContainerLayout *layout,
						  GnomeIconContainerLayoutForeachFunc callback,
						  gpointer callback_data);

#endif /* _GNOME_ICON_CONTAINER_LAYOUT_H */
