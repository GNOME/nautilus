/*  -*- Mode: C; c-set-style: linux; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * Nautilus shell - mini icon setting routine
 * 
 * Copyright (C) 1999 Anders Carlsson
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef NTL_MINIICON_H
#define NTL_MINIICON_H

#include <gdk/gdk.h>

/* In GNOME 2.0 this function will be in the libraries */
void
nautilus_set_mini_icon(GdkWindow *window,
                       GdkPixmap *pixmap,
                       GdkBitmap *mask);


#endif
