/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-icon-factory-private.h: Private interface for use within
   the icon factory code.
 
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
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#ifndef NAUTILUS_ICON_FACTORY_PRIVATE_H
#define NAUTILUS_ICON_FACTORY_PRIVATE_H

#include "nautilus-icon-factory.h"

/* For now, images are used themselves as thumbnails when they are
 * below this threshold size. Later we might have to have a more
 * complex rule about when to use an image for itself.
 */
#define SELF_THUMBNAIL_SIZE_THRESHOLD   16384

void nautilus_icon_factory_remove_by_uri (const char *uri);

/* Convenience routine to return the appropriate thumbnail frame
 */
GdkPixbuf * nautilus_icon_factory_get_thumbnail_frame (gboolean anti_aliased);

#endif /* NAUTILUS_ICON_FACTORY_PRIVATE_H */
