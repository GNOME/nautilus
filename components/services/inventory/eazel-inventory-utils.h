/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus
 * Copyright (C) 2000 Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: J Shane Culpepper <pepper@eazel.com>
 *          Ian McKellar <ian@eazel.com>
 *
 * This is the header file for the service inventory stuff
 *
 */

#ifndef EAZEL_INVENTORY_UTILS_H
#define EAZEL_INVENTORY_UTILS_H

#include <gnome.h>
#include "eazel-package-system.h"

#define KEY_GCONF_EAZEL_INVENTORY_MACHINE_NAME "/apps/eazel-trilobite/machine_name"

gboolean eazel_gather_inventory				(void);
gboolean update_gconf_inventory_digest			(unsigned char          value[16]);
gchar *eazel_inventory_local_path			(void);
void eazel_inventory_update_md5                         (void);
void eazel_inventory_clear_md5				(void);

#endif /* EAZEL_INVENTORY_UTILS_H */
