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
 * Authors: J Shane Culpepper
 */

#ifndef NAUTILUS_SERVICE_INSTALL_VIEW_H
#define NAUTILUS_SERVICE_INSTALL_VIEW_H

#include <libnautilus/nautilus-view.h>
#include <gtk/gtk.h>
#include <eazel-package-system-types.h>
#include <eazel-install-corba-types.h>
#include <eazel-install-corba-callback.h>
#include <eazel-install-problem.h>
#include "libtrilobite/libtrilobite.h"

typedef struct _NautilusServiceInstallView NautilusServiceInstallView;
typedef struct _NautilusServiceInstallViewClass NautilusServiceInstallViewClass;

#define NAUTILUS_TYPE_SERVICE_INSTALL_VIEW		(nautilus_service_install_view_get_type ())
#define NAUTILUS_SERVICE_INSTALL_VIEW(obj)		(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_SERVICE_INSTALL_VIEW, NautilusServiceInstallView))
#define NAUTILUS_SERVICE_INSTALL_VIEW_CLASS (klass)	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SERVICE_INSTALL_VIEW, NautilusServiceInstallViewClass))
#define NAUTILUS_IS_SERVICE_INSTALL_VIEW(obj)		(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_SERVICE_INSTALL_VIEW))
#define NAUTILUS_IS_SERVICE_INSTALL_VIEW_CLASS (klass)	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SERVICE_INSTALL_VIEW))

typedef struct _NautilusServiceInstallViewDetails NautilusServiceInstallViewDetails;

struct _NautilusServiceInstallView {
        GtkEventBox				parent;
        NautilusServiceInstallViewDetails	*details;
};

struct _NautilusServiceInstallViewClass {
        GtkVBoxClass				parent_class;
};

typedef struct {
	char *package_name;
	GtkWidget *label;
	GtkWidget *progress_bar;
	GtkWidget *progress_label;
	GtkWidget *vbox;	/* [ progress_bar, progress_label ] */
	GtkWidget *hbox;	/* [ label, padding, vbox ] */
	GtkWidget *line;
} InstallMessage;

/* A NautilusContentView's private information. */
struct _NautilusServiceInstallViewDetails {
	char            *uri;
	NautilusView    *nautilus_view;
	GtkWidget       *form;
	GtkWidget       *form_title;
	GtkWidget       *package_name;
	GtkWidget       *package_details;
	GtkWidget       *package_summary;
	GtkWidget       *package_version;
	GtkWidget       *total_progress_bar;
	GtkWidget       *overall_feedback_text;
	GtkWidget	*message_box;
	GtkWidget       *current_feedback_text;
	InstallMessage	*current_im;
	GtkWidget	*pane;
	GtkWidget	*middle_title;

	char		*current_rpm;
	int		current_package;
	char		*remembered_password;
	int		password_attempts;
	guint		cylon_timer;
	int		using_local_file;
	gboolean	failure;
	gboolean	cancelled;
	gboolean	already_installed;
	int		last_k;			/* used to avoid flickering the KB count so much */
	gboolean	cancelled_before_downloads;
	unsigned long	download_bytes_total;
	unsigned long	download_bytes_sofar;

	EazelInstallCallback *installer;
	EazelInstallProblem *problem;
	TrilobiteRootClient *root_client;
	GList		    *categories;	

	gboolean core_package;

	GList *problem_cases;
	GList *desktop_files;

	GList *message;		/* GList<InstallMessage *> */
	GHashTable *deps;	/* package(char *) => package that needs it(char *) */
};


/* GtkObject support */
GtkType		nautilus_service_install_view_get_type			(void);

/* Component embedding support */
NautilusView 	*nautilus_service_install_view_get_nautilus_view	(NautilusServiceInstallView *view);

/* URI handling */
void		nautilus_service_install_view_load_uri			(NautilusServiceInstallView	*view,
									 const char			*uri);

#endif /* NAUTILUS_SERVICE_INSTALL_VIEW_H */

