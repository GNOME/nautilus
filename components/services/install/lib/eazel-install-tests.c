/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
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
 * Authors: J Shane Culpepper <pepper@eazel.com>
 */

/* eazel-install - services command line install/update/uninstall
 * component.  This program will parse the eazel-services-config.xml
 * file and install a services generated package-list.xml.
 */

#include "eazel-install-tests.h"
#include <config.h>

void 
dump_package_list (PackageData* pkg) {
	g_print (_("*** Begin pkg dump ***\n"));
	g_print ("name = %s\n", pkg->name);
	g_print ("version = %s\n", pkg->version);
	g_print ("minor = %s\n", pkg->minor);
	g_print ("archtype = %s\n", pkg->archtype);
	g_print ("bytesize = %d\n", pkg->bytesize);
	g_print ("summary = %s\n", pkg->summary);
	g_print (_("*** End pkg dump ***\n"));
} /* end dump_package_list */
