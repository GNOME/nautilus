/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-services-support.h - Functions for using services from Nautilus.

   Copyright (C) 2001 Eazel, Inc.

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

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef NAUTILUS_SERVICES_H
#define NAUTILUS_SERVICES_H

#include <glib.h>

gboolean nautilus_services_are_enabled              (void);
char *   nautilus_services_get_summary_uri          (void);
char *   nautilus_services_get_user_name            (void);
char *   nautilus_services_get_online_storage_uri   (void);
char *   nautilus_services_get_software_catalog_uri (void);

#endif /* NAUTILUS_SERVICES_H */

