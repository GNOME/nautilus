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
 * Author: Andy Hertzfeld
 */

/* private header file for the rpm view component */

#ifndef NAUTILUS_RPM_VIEW_PRIVATE_H
#define NAUTILUS_RPM_VIEW_PRIVATE_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef EAZEL_SERVICES
#include "libeazelinstall.h"
#include "libtrilobite/libtrilobite.h"
#include "eazel-package-system.h"
#include "nautilus-rpm-view-install.h"
#endif /* EAZEL_SERVICES */        

struct NautilusRPMViewDetails {
	char *current_uri;
	char *package_name;
	
	NautilusView *nautilus_view;
        
	GtkWidget *package_image;
	GtkWidget *package_title;
	GtkWidget *package_release;
	GtkWidget *package_summary;
	GtkWidget *package_size;
	GtkWidget *package_idate;
	GtkWidget *package_license;
	GtkWidget *package_bdate;
	GtkWidget *package_distribution;
	GtkWidget *package_vendor;      
	GtkWidget *package_description;    
        
	GtkWidget *package_installed_message;
	GtkWidget *package_install_button;
	GtkWidget *package_update_button;
	GtkWidget *package_uninstall_button;
	GtkWidget *package_verify_button;
	
	GtkWidget *verify_window;
	
	GtkVBox   *package_container;
	GtkWidget *go_to_button;
	
	GtkWidget *package_file_tree;
	GNode *filename_tree;
	GtkCTreeNode *selected_file;
	gboolean  package_installed;
	
	int background_connection;
	int file_count;
	int last_file_index;

	gboolean verify_success;

#ifdef EAZEL_SERVICES
        /* for installing an rpm */
	EazelInstallCallback *installer;
	EazelPackageSystem *package_system;
	TrilobiteRootClient *root_client;

	PackageData *package;
	/* for password queries */
	char *remembered_password;
	int password_attempts;
#endif
};

#endif	/* NAUTILUS_RPM_VIEW_PRIVATE_H */
