/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000, 2001 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Maciej Stachowiak
 */

#include <config.h>

#include "nautilus-tree-view-iids.h"
#include "nautilus-tree-view.h"
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus/nautilus-view-standard-main.h>

int
main (int argc, char *argv[])
{
	return nautilus_view_standard_main ("nautilus-tree-view",
					    VERSION,
					    PACKAGE,
					    GNOMELOCALEDIR,
					    argc,
					    argv,
					    TREE_VIEW_FACTORY_IID,
					    TREE_VIEW_IID,
					    nautilus_view_create_from_get_type_function,
					    nautilus_global_preferences_initialize,
					    nautilus_tree_view_get_type);
}
