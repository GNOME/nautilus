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
 * component.  This program will parse the eazel-configuration.xml
 * file and install a services generated packages.xml.
 */

 #ifndef __EAZEL_INSTALL_LIB_H__
 #define __EAZEL_INSTALL_LIB_H__

#include <popt-gnome.h>
#include <gnome.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>
#include <rpm/rpmlib.h>

typedef enum _URLType URLType;
typedef struct _InstallOptions InstallOptions;
typedef struct _InstallList InstallList;
typedef struct _CategoryData CategoryData;
typedef struct _PackageData PackageData;

enum _URLType {
	PROTOCOL_LOCAL,
	PROTOCOL_HTTP,
	PROTOCOL_FTP
};

struct _InstallOptions {
	URLType protocol;
	gboolean mode_debug;		/* Internal testing mode for debugging */
	gboolean mode_test;
	gboolean mode_verbose;
	gboolean mode_silent;
	gboolean mode_depend;
	gboolean mode_uninstall;
	gboolean mode_update;
	guint port_number;
	char* rpmrc_file;			/* Points to the rpmrc file */
	char* pkg_list_file;		/* The URI pointing to packages.xml */
	char* rpm_storage_dir;		/* The URI pointing to the rpm location */
	char* install_tmpdir;		/* Location to copy rpm downloads before installing */
};

struct _InstallList {
	GList Categories;
	char* rpm_storage_uri;
	char* tmp_dir;
	char* rpm_flags;
};

struct _CategoryData {
	char* name;
	GList* Packages;
};

struct _PackageData {
	char* rpm_name;
	char* name;
	char* summary;
	char* version;
	gboolean srcfile;
	int bytesize;
	GList* SoftDepends;
	GList* HardDepends;
};

gboolean check_for_root_user (void);
gboolean check_for_redhat (void);
char* xml_get_value (xmlNode* node, const char* name);
InstallOptions* init_default_install_configuration (const char* config_file);
InstallOptions* init_default_install_configuration_test (void);
GList* fetch_xml_package_list_local (const char* pkg_list_file);
gboolean install_new_packages (InstallOptions* iopts);
gboolean uninstall_packages (InstallOptions* iopts);
void dump_install_options (InstallOptions* iopts);
void dump_package_list (PackageData* pkg);
void free_categories (GList* categories);
gboolean create_default_configuration_metafile (void);

 #endif /* __EAZEL_INSTALL_LIB_H__ */