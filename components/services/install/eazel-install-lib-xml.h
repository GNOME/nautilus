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
 * 			Joe Shaw <joe@helixcode.com>
 */

/* eazel-install - services command line install/update/uninstall
 * component.  This program will parse the eazel-services-config.xml
 * file and install a services generated package-list.xml.
 */

#ifndef __EAZEL_INSTALL_LIB_XML_H__
#define __EAZEL_INSTALL_LIB_XML_H__

#include <errno.h>
#include <sys/stat.h>
#include <gnet/gnet.h>
#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>

#include "eazel-install-lib.h"

char* xml_get_value (xmlNode* node, const char* name);
InstallOptions* init_default_install_configuration (const char* config_file);
GList* parse_local_xml_package_list (const char* pkg_list_file);
gboolean http_fetch_xml_package_list (const char* hostname,
									  int port,
									  const char* path,
									  const char* pkg_list_file);
void free_categories (GList* categories);

#endif /* __EAZEL_INSTALL_LIB_XML_H__ */