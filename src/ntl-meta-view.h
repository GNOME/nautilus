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
/* ntl-meta-view.h: Interface of the object representing a meta/navigation view. Derived from NautilusView. */

#ifndef NAUTILUS_META_VIEW_H
#define NAUTILUS_META_VIEW_H 1

#include "ntl-view.h"

#define NAUTILUS_TYPE_META_VIEW (nautilus_meta_view_get_type())
#define NAUTILUS_META_VIEW(obj)	        (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_META_VIEW, NautilusMetaView))
#define NAUTILUS_META_VIEW_CLASS(klass)      (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_META_VIEW, NautilusMetaViewClass))
#define NAUTILUS_IS_META_VIEW(obj)	        (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_META_VIEW))
#define NAUTILUS_IS_META_VIEW_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_META_VIEW))

typedef struct _NautilusMetaView NautilusMetaView;

typedef struct {
  NautilusViewClass parent_spot;

  NautilusViewClass *parent_class;
} NautilusMetaViewClass;

struct _NautilusMetaView {
  NautilusView parent_object;
};

GtkType nautilus_meta_view_get_type(void);
NautilusMetaView *nautilus_meta_view_new(void);
const char *nautilus_meta_view_get_label(NautilusMetaView *nview);

#endif
