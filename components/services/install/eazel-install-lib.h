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

#ifndef __EAZEL_INSTALL_LIB_H__
#define __EAZEL_INSTALL_LIB_H__

#include <gnome.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef enum _URLType URLType;
typedef struct _InstallOptions InstallOptions;
typedef struct _CategoryData CategoryData;
typedef struct _PackageData PackageData;

enum _URLType {
	PROTOCOL_LOCAL,
	PROTOCOL_HTTP,
	PROTOCOL_FTP
};

struct _InstallOptions {
	URLType protocol;			/* Specifies local, ftp, or http */ 
	gboolean mode_debug;		/* Internal testing mode for debugging */
	gboolean mode_test;			/* dry run mode */
	gboolean mode_verbose;		/* print extra information */
	gboolean mode_silent;		/* FIXME print all information to a logfile */
	gboolean mode_depend;		/* FIXME print all dependancies */
	gboolean mode_uninstall;	/* Uninstall the package list */
	gboolean mode_update;		/* If package is already installed, update it */
	guint port_number;			/* Connection port */
	char* hostname;
	char* rpmrc_file;			/* Points to the rpmrc file */
	char* pkg_list_file;		/* Absolute path to package-list.xml */
	char* rpm_storage_dir;		/* Absolute path to remote RPM directory */
	char* install_tmpdir;		/* Location to copy rpm downloads before installing */
};

struct _CategoryData {
	char* name;
	GList* Packages;
};

struct _PackageData {
	char* name;
	char* version;
	char* minor;
	char* archtype;
	int bytesize;
	char* summary;
	GList* SoftDepends;
	GList* HardDepends;
};

#endif /* __EAZEL_INSTALL_LIB_H__ */