/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999 Red Hat, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */
/* ntl-types.h: Declarations of basic types */

#ifndef NTL_TYPES_H
#define NTL_TYPES_H 1

#include <gtk/gtk.h>
#include <libnautilus/libnautilus.h>
#include <libgnomevfs/gnome-vfs.h>

typedef char *NautilusLocationReference;

typedef struct {
  Nautilus_NavigationInfo navinfo;

  gpointer requesting_view;

  GnomeVFSFileInfo *vfs_fileinfo;

  char *content_iid;
  GSList *meta_iids;
} NautilusNavigationInfo;

#endif
