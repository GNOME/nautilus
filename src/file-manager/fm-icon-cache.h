/*  -*- Mode: C; c-set-style: linux; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * GNOME File Manager: Icon Cache
 * 
 * Copyright (C) 1999 Red Hat Inc.
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
#ifndef FM_ICON_CACHE_H
#define FM_ICON_CACHE_H 1

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnomevfs/gnome-vfs.h>
#include <glib.h>

typedef struct _FMIconCache FMIconCache;

FMIconCache *fm_icon_cache_new       (const char             *theme_name);
void         fm_icon_cache_destroy   (FMIconCache            *fmic);
void         fm_icon_cache_set_theme (FMIconCache            *fmic,
                                      const char             *theme_name);
/* Ownership of a refcount in this pixbuf comes with the deal */
GdkPixbuf *  fm_icon_cache_get_icon  (FMIconCache            *fmic,
                                      const GnomeVFSFileInfo *info);

FMIconCache* fm_get_current_icon_cache        (void);

#endif



