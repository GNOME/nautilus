/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-  c-set-style: linux */

/* nautilus-gconf.h - GConf-related functions

   Copyright (C) 2000 Red Hat, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Havoc Pennington <hp@redhat.com>
*/

#ifndef NAUTILUS_GCONF_H
#define NAUTILUS_GCONF_H 1

#include <gconf/gconf-client.h>

/* Returns global GConfClient object. GConfClient stores our
   client-side cache of configuration data, and automatically creates
   error dialogs in response to GConf errors.

   Note that gnome-libs 2 provides a global GConfClient object also.
   When we port to gnome-libs 2, we may want to change these functions
   to simply return the GNOME object.  */

GConfClient *nautilus_gconf_client_get (void);

/* Shut down the client cleanly, on application exit */
void         nautilus_gconf_shutdown   (void);

#endif /* NAUTILUS_GCONF_H */
