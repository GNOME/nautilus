/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-thumbnails-jpeg.h: Fast loading of scaled jpeg images
 
   Copyright (C) 2001 Red Hat, Inc
  
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
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#ifndef NAUTILUS_THUMBNAILS_JPEG_H
#define NAUTILUS_THUMBNAILS_JPEG_H

#include <gdk-pixbuf/gdk-pixbuf.h>

GdkPixbuf *nautilus_thumbnail_load_scaled_jpeg (const char *uri,
						int         target_width,
						int         target_heigh);

#endif /* NAUTILUS_THUMBNAILS_JPEG_H */
