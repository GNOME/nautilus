/*
   eel-graphic-effects.h: Pixmap manipulation routines for graphical effects.

   Copyright (C) 2000 Eazel, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
  
   You should have received a copy of the GNU Library General Public
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
 
   Authors: Andy Hertzfeld <andy@eazel.com>
 */

#pragma once

#include <gdk/gdk.h>

GdkTexture *eel_create_spotlight_texture (GdkTexture *texture);
