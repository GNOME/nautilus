/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
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
/* ntl-uri-map.h: Interface for mapping a location change request to a set of views and actual URL to be loaded. */

#ifndef NAUTILUS_URI_MAP_H
#define NAUTILUS_URI_MAP_H 1

#include "ntl-types.h"
#include "ntl-view.h"

void nautilus_navinfo_init(void);
NautilusNavigationInfo *nautilus_navinfo_new(NautilusNavigationInfo *navinfo,
					     Nautilus_NavigationRequestInfo *nri,
                                             Nautilus_NavigationInfo *old_navinfo,
					     NautilusView *requesting_view);
void nautilus_navinfo_free(NautilusNavigationInfo *navinfo);

#endif
