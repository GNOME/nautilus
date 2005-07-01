/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  Nautilus
 *
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
 *  Author : Mr Jamie McCracken (jamiemcc at blueyonder dot co dot uk)
 *
 */
#ifndef _NAUTILUS_PLACES_SIDEBAR_H
#define _NAUTILUS_PLACES_SIDEBAR_H

#include <gtk/gtktreeview.h>
#include <libnautilus-private/nautilus-view.h>
#include <libnautilus-private/nautilus-window-info.h>
#include <gtk/gtkscrolledwindow.h>

#define NAUTILUS_PLACES_SIDEBAR_ID    "NautilusPlacesSidebar"

#define NAUTILUS_TYPE_PLACES_SIDEBAR (nautilus_places_sidebar_get_type ())
#define NAUTILUS_PLACES_SIDEBAR(obj) (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_PLACES_SIDEBAR, NautilusPlacesSidebar))

GType nautilus_places_sidebar_get_type (void);
void nautilus_places_sidebar_register (void);

#endif
