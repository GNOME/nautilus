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

#include "ntl-miniicon.h"

#include <gdk/gdkprivate.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

void
nautilus_set_mini_icon(GdkWindow *window,
                       GdkPixmap *pixmap,
                       GdkBitmap *mask)
{
        GdkAtom icon_atom;
        long data[2];
 
        g_return_if_fail (window != NULL);
        g_return_if_fail (pixmap != NULL);

        data[0] = ((GdkPixmapPrivate *)pixmap)->xwindow;
        if (mask)
                data[1] = ((GdkPixmapPrivate *)mask)->xwindow;
        else
                data[1] = 0;

        icon_atom = gdk_atom_intern ("KWM_WIN_ICON", FALSE);
        gdk_property_change (window, icon_atom, icon_atom, 
                             32,GDK_PROP_MODE_REPLACE,
                             (guchar *)data, 2);
        
}
