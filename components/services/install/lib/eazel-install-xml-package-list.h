/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 * Copyright (C) 2000 Helix Code, Inc
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
 *          Joe Shaw <joe@helixcode.com>
 */

/* eazel-install - services command line install/update/uninstall
 * component.  This program will parse the eazel-services-config.xml
 * file and install a services generated package-list.xml.
 */

#ifndef __EAZEL_SERVICES_XML_PACKAGE_LISTH__
#define __EAZEL_SERVICES_XML_PACKAGE_LIST_H__

#include "eazel-install-types.h"
#include <libtrilobite/helixcode-utils.h>
#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>

GList* parse_local_xml_package_list (const char* pkg_list_file);
void free_categories (GList* categories);
gboolean generate_xml_package_list (const char* pkg_template_file,
                                    const char* target_file);

#endif /* __EAZEL_SERVICES_XML_PACKAGE_LIST_H__ */
