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
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 */

#include "nautilus-service-install.h"
#include <libtrilobite/helixcode-utils.h>
#include <libeazelinstall.h>

#define OAF_ID "OAFIID:trilobite_eazel_install_service:8ff6e815-1992-437c-9771-d932db3b4a17"

static void 
nautilus_service_install_download_progress_signal (EazelInstallCallback *service, 
					    const char *name,
					    int amount, 
					    int total,
					    NautilusServiceInstallView *view) 
{
	fprintf (stdout, "Download progress - %s %% %f\r",  name,
		 (total ? ((float)
			   ((((float) amount) / total) * 100))
		  : 100.0));
	fflush (stdout);
	if (amount == total && total!=0) {
		fprintf (stdout, "\n");
	}
}

static void 
nautilus_service_install_progress_signal (EazelInstallCallback *service, 
					   const PackageData *pack,
					   int amount, 
					   int total,
					   NautilusServiceInstallView *view) 
{
	fprintf (stdout, "Install progress - %s %% %f\r", pack->name,
		 (total ? ((float)
			   ((((float) amount) / total) * 100))
		  : 100.0));
	fflush (stdout);
	if (amount == total && total!=0) {
		fprintf (stdout, "\n");
	}
}

static void
nautilus_service_install_download_failed (EazelInstallCallback *service, 
				   const char *name,
				   NautilusServiceInstallView *view)
{
	fprintf (stdout, "Download of %s FAILED\n", name);
}

/*
  This dumps the entire tree for the failed package.
 */
static void
nautilus_service_install_failed_helper (EazelInstallCallback *service,
					 const PackageData *pd,
					 gchar *indent,
					 NautilusServiceInstallView *view)
{
	GList *iterator;

	if (pd->toplevel) {
		fprintf (stdout, "\n***The package %s failed. Here's the dep tree\n", pd->name);
	}
	switch (pd->status) {
	case PACKAGE_DEPENDENCY_FAIL:
		fprintf (stdout, "%s-%s FAILED\n", indent, rpmfilename_from_packagedata (pd));
		break;
	case PACKAGE_CANNOT_OPEN:
		fprintf (stdout, "%s-%s NOT FOUND\n", indent, rpmfilename_from_packagedata (pd));
		break;		
	case PACKAGE_SOURCE_NOT_SUPPORTED:
		fprintf (stdout, "%s-%s is a source package\n", indent, rpmfilename_from_packagedata (pd));
		break;
	case PACKAGE_BREAKS_DEPENDENCY:
		fprintf (stdout, "%s-%s breaks\n", indent, rpmfilename_from_packagedata (pd));
		break;
	default:
		fprintf (stdout, "%s-%s\n", indent, rpmfilename_from_packagedata (pd));
		break;
	}
	for (iterator = pd->soft_depends; iterator; iterator = iterator->next) {			
		PackageData *pack;
		char *indent2;
		indent2 = g_strconcat (indent, iterator->next ? " |" : "  " , NULL);
		pack = (PackageData*)iterator->data;
		nautilus_service_install_failed_helper (service, pack, indent2, view);
		g_free (indent2);
	}
	for (iterator = pd->breaks; iterator; iterator = iterator->next) {			
		PackageData *pack;
		char *indent2;
		indent2 = g_strconcat (indent, iterator->next ? " |" : "  " , NULL);
		pack = (PackageData*)iterator->data;
		nautilus_service_install_failed_helper (service, pack, indent2, view);
		g_free (indent2);
	}
}

static void
nautilus_service_install_failed (EazelInstallCallback *service,
				  const PackageData *pd,
				  NautilusServiceInstallView *view)
{
	nautilus_service_install_failed_helper (service, pd, "", view);
}


static void
nautilus_service_install_dependency_check (EazelInstallCallback *service,
				    const PackageData *package,
				    const PackageData *needs,
				    NautilusServiceInstallView *view) 
{
	g_message ("Doing dependency check for %s - need %s\n", package->name, needs->name);
}

static void
nautilus_service_install_done (EazelInstallCallback *service,
				NautilusServiceInstallView *view)
{
	char *tmp;
	eazel_install_callback_destroy (GTK_OBJECT (service));
	tmp = g_strdup (view->details->uri);
	nautilus_service_install_view_load_uri (view, tmp);
	g_free (tmp);
}

void 
nautilus_service_install_view_install_package_callback (GtkWidget *widget,
                                                        NautilusServiceInstallView *view)
{
	GList *packages;
	GList *categories;
	CORBA_Environment ev;
	EazelInstallCallback *cb;		

	CORBA_exception_init (&ev);

	packages = NULL;
	categories = NULL;

	{
		char *ptr;
		CategoryData *category;
		PackageData *pack;

		/* Find the :// of the url and skip to after it */
		ptr = strstr (view->details->uri, "file://");
		ptr += strlen ("file://");

		/* make a package and add to it to a categorylist */
		pack = packagedata_new ();
		pack->filename = g_strdup (ptr);
		
		category = g_new0 (CategoryData, 1);
		category->packages = g_list_prepend (NULL, pack);
		categories = g_list_prepend (NULL, category);
	}

	/* Check that we're on a redhat system */
	if (check_for_redhat () == FALSE) {
		fprintf (stderr, "*** This tool can only be used on RedHat.\n");
	}

	cb = eazel_install_callback_new ();
	
	Trilobite_Eazel_Install__set_protocol (eazel_install_callback_corba_objref (cb), Trilobite_Eazel_PROTOCOL_HTTP, &ev);
	if (check_for_root_user() == FALSE) {
		fprintf (stderr, "*** This tool requires root access, switching to test mode\n");
		Trilobite_Eazel_Install__set_test_mode (eazel_install_callback_corba_objref (cb), TRUE, &ev); 
	} else {
		Trilobite_Eazel_Install__set_test_mode (eazel_install_callback_corba_objref (cb), FALSE, &ev); 
	}
	Trilobite_Eazel_Install__set_tmp_dir (eazel_install_callback_corba_objref (cb), "/tmp/eazel-install", &ev);
	Trilobite_Eazel_Install__set_server (eazel_install_callback_corba_objref (cb), "testmachine.eazel.com", &ev);
	Trilobite_Eazel_Install__set_server_port (eazel_install_callback_corba_objref (cb), 80, &ev);

	gtk_signal_connect (GTK_OBJECT (cb), "download_progress", nautilus_service_install_download_progress_signal, view);
	gtk_signal_connect (GTK_OBJECT (cb), "install_progress", nautilus_service_install_progress_signal, view);
	gtk_signal_connect (GTK_OBJECT (cb), "install_failed", nautilus_service_install_failed, view);
	gtk_signal_connect (GTK_OBJECT (cb), "download_failed", nautilus_service_install_download_failed, view);
	gtk_signal_connect (GTK_OBJECT (cb), "dependency_check", nautilus_service_install_dependency_check, view);
	gtk_signal_connect (GTK_OBJECT (cb), "done", nautilus_service_install_done, view);
	
	eazel_install_callback_install_packages (cb, categories, &ev);
	
	CORBA_exception_free (&ev);               
}

void 
nautilus_service_install_view_uninstall_package_callback (GtkWidget *widget,
			                		  NautilusServiceInstallView *view)
{
	GList *packages;
	GList *categories;
	CORBA_Environment ev;
	EazelInstallCallback *cb;		

	CORBA_exception_init (&ev);

	packages = NULL;
	categories = NULL;

	{
		CategoryData *category;
		PackageData *pack;
		pack = gtk_object_get_data (GTK_OBJECT (view), "packagedata");
		
		category = g_new0 (CategoryData, 1);
		category->packages = g_list_prepend (NULL, pack);
		categories = g_list_prepend (NULL, category);
	}

	/* Check that we're on a redhat system */
	if (check_for_redhat () == FALSE) {
		fprintf (stderr, "*** This tool can only be used on RedHat.\n");
	}

	cb = eazel_install_callback_new ();
	
	Trilobite_Eazel_Install__set_protocol (eazel_install_callback_corba_objref (cb), Trilobite_Eazel_PROTOCOL_HTTP, &ev);
	if (check_for_root_user() == FALSE) {
		fprintf (stderr, "*** This tool requires root access, switching to test mode\n");
		Trilobite_Eazel_Install__set_test_mode (eazel_install_callback_corba_objref (cb), TRUE, &ev); 
	} else {
		Trilobite_Eazel_Install__set_test_mode (eazel_install_callback_corba_objref (cb), FALSE, &ev); 
	}
	Trilobite_Eazel_Install__set_tmp_dir (eazel_install_callback_corba_objref (cb), "/tmp/eazel-install", &ev);
	Trilobite_Eazel_Install__set_server (eazel_install_callback_corba_objref (cb), "testmachine.eazel.com", &ev);
	Trilobite_Eazel_Install__set_server_port (eazel_install_callback_corba_objref (cb), 80, &ev);

	gtk_signal_connect (GTK_OBJECT (cb), "download_progress", nautilus_service_install_download_progress_signal, view);
	gtk_signal_connect (GTK_OBJECT (cb), "uninstall_progress", nautilus_service_install_progress_signal, view);
	gtk_signal_connect (GTK_OBJECT (cb), "uninstall_failed", nautilus_service_install_failed, view);
	gtk_signal_connect (GTK_OBJECT (cb), "dependency_check", nautilus_service_install_dependency_check, view);
	gtk_signal_connect (GTK_OBJECT (cb), "done", nautilus_service_install_done, view);
	
	eazel_install_callback_uninstall_packages (cb, categories, &ev);
	
	CORBA_exception_free (&ev);               
}
