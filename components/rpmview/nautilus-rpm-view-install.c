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

#include "nautilus-rpm-view-install.h"
#include "libtrilobite/libtrilobite.h"
#include "libeazelinstall.h"
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include <libnautilus-extensions/nautilus-password-dialog.h>
#include "libtrilobite/libtrilobite.h"
#include "nautilus-rpm-view-private.h"
#ifdef EAZEL_SERVICES
#include "eazel-inventory.h"
#endif

#include <eazel-install-problem.h>

#define OAF_ID "OAFIID:trilobite_eazel_install_service:8ff6e815-1992-437c-9771-d932db3b4a17"

static void 
nautilus_rpm_view_download_progress_signal (EazelInstallCallback *service, 
					    const PackageData *pack,
					    int amount, 
					    int total,
					    NautilusRPMView *rpm_view) 
{
#if 0
	fprintf (stdout, "DEBUG: Download progress - %s %% %f\r",  name,
		 (total ? ((float)
			   ((((float) amount) / total) * 100))
		  : 100.0));
	fflush (stdout);
	if (amount == total && total!=0) {
		fprintf (stdout, "\n");
	}
	nautilus_view_report_load_underway (nautilus_rpm_view_get_view (rpm_view));
#endif
}

static void 
nautilus_rpm_view_install_progress_signal (EazelInstallCallback *service, 
					   const PackageData *pack,
					   int package_num, int num_packages, 
					   int amount, int total,
					   int total_size_completed, int total_size, 
					   NautilusRPMView *rpm_view) 
{
#if 0
	double progress;

	fprintf (stdout, "DEBUG: Install progress - %s (%d/%d), (%d/%d)b - (%d/%d) %% %f\r", 
		 pack->name, 
		 package_num, num_packages,
		 total_size_completed, total_size,
		 amount, total,
		 (total ? ((float)
			   ((((float) amount) / total) * 100))
		  : 100.0));
	fflush (stdout);

	if (amount == total && total!=0) {
		fprintf (stdout, "\n");
	}

	nautilus_view_report_load_underway (nautilus_rpm_view_get_view (rpm_view));

	progress = total==amount ? 1.0 : (double)(((double)amount)/total);
	/* nautilus_view_report_load_progress (nautilus_rpm_view_get_view (rpm_view), progress); */
#endif
}

static void
nautilus_rpm_view_download_failed (EazelInstallCallback *service, 
				   const PackageData *pack,
				   NautilusRPMView *rpm_view)
{
	g_assert (pack->name != NULL);
	trilobite_debug ("Download of %s FAILED", pack->name);
}

static char*
get_detailed_errors (EazelInstallCallback *service,
		     PackageData *pack, 
		     gboolean uninstalling)
{
	char *result;
	GList *stuff;
	GString *message;
	EazelInstallProblem *problem;

	message = g_string_new ("");
	if (uninstalling==FALSE) {
		g_string_sprintfa (message, _("Installing %s failed because of the following issue(s):\n"), pack->name);
	} else {
		g_string_sprintfa (message, _("Uninstalling %s failed because of the following issue(s):\n"), pack->name);
	}
	
	problem = EAZEL_INSTALL_PROBLEM (gtk_object_get_data (GTK_OBJECT (service), "problem-handler"));
	stuff = eazel_install_problem_tree_to_string (problem, pack, uninstalling);
	if (stuff) {
		GList *iterator;
		for (iterator = stuff; iterator; iterator = g_list_next (iterator)) {
			g_string_sprintfa (message, "\n\t\xB7 %s", (char*)iterator->data);
		}
	}

	result = message->str;
	g_string_free (message, FALSE);
	return result;
}

static void
nautilus_rpm_view_install_failed (EazelInstallCallback *service,
				  PackageData *pd,
				  NautilusRPMView *rpm_view)
{
	char *detailed;

	detailed = get_detailed_errors (service, pd, FALSE);
	gtk_object_set_data (GTK_OBJECT (rpm_view), "details", detailed);
}

static void
nautilus_rpm_view_uninstall_failed (EazelInstallCallback *service,
				    PackageData *pd,
				    NautilusRPMView *rpm_view)
{
	char *detailed;

	detailed = get_detailed_errors (service, pd, TRUE);
	gtk_object_set_data (GTK_OBJECT (rpm_view), "details", detailed);
}

static void
nautilus_rpm_view_dependency_check (EazelInstallCallback *service,
				    const PackageData *package,
				    const PackageData *needs,
				    NautilusRPMView *rpm_view) 
{
	char *a, *b;

	a = packagedata_get_readable_name (package);
	b = packagedata_get_readable_name (needs);
	g_message ("Doing dependency check for %s - needs %s\n", a, b);
	g_free (a);
	g_free (b);
}

/* get rid of the installer and root client, and reactivate buttons */
static void
nautilus_rpm_view_finished_working (NautilusRPMView *rpm_view)
{
	eazel_install_callback_unref (GTK_OBJECT (rpm_view->details->installer));
	rpm_view->details->installer = NULL;
	trilobite_root_client_unref (GTK_OBJECT (rpm_view->details->root_client));
	rpm_view->details->root_client = NULL;

	gtk_widget_set_sensitive (GTK_WIDGET (rpm_view->details->package_install_button), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (rpm_view->details->package_uninstall_button), TRUE);

	rpm_view->details->password_attempts = 0;
}

#ifdef EAZEL_SERVICES
static void
inventory_service_callback (EazelInventory *inventory,
	  		 gboolean succeeded,
	  		 gpointer callback_data)
{
	NautilusRPMView *rpm_view;
	char *tmp;

	rpm_view = NAUTILUS_RPM_VIEW (callback_data);

	gtk_object_unref (GTK_OBJECT (inventory));

	tmp = g_strdup (nautilus_rpm_view_get_uri (rpm_view));
	nautilus_rpm_view_load_uri (rpm_view, tmp);
	g_free (tmp);
}
#endif


static void
nautilus_rpm_view_install_done (EazelInstallCallback *service,
				gboolean result,
				NautilusRPMView *rpm_view)
{
#ifdef EAZEL_SERVICES
	EazelInventory *inventory_service;
#else
	char *tmp;	
#endif

	if (!result) {
		char *dialog_title;
		char *terse;
		char *detailed;

		GnomeDialog *d;
		GtkWidget *window;

		detailed = (char *) gtk_object_get_data (GTK_OBJECT (rpm_view), "details");
	
		if (nautilus_rpm_view_get_installed (rpm_view)) {
			terse = g_strdup (_("Uninstall failed..."));
			dialog_title = g_strdup (_("Uninstall Failed"));
		} else {
			terse = g_strdup (_("Install failed..."));
			dialog_title = g_strdup (_("Install Failed"));
		}

		window = gtk_widget_get_toplevel (GTK_WIDGET (rpm_view));
		g_assert (window);
		g_assert (GTK_IS_WINDOW (window));
		d = nautilus_show_error_dialog_with_details (terse, 
							     dialog_title,
							     detailed,
							     GTK_WINDOW (window));
			
		/* should this be gnome_dialog_run_close ?
		   Changed it when fixing 7251 */
		gnome_dialog_run (d);
		g_free (terse);
		g_free (dialog_title);
		g_free (detailed);
		/* nautilus_view_report_load_failed (nautilus_rpm_view_get_view (rpm_view)); */
		nautilus_view_report_load_complete (nautilus_rpm_view_get_view (rpm_view));
	} else {
		nautilus_view_report_load_complete (nautilus_rpm_view_get_view (rpm_view));
	}

	{
		CORBA_Environment ev;
		CORBA_exception_init (&ev);
		eazel_install_callback_delete_files (service, &ev);
		CORBA_exception_free (&ev);
	}
	
	nautilus_rpm_view_finished_working (rpm_view);
	
#ifdef EAZEL_SERVICES
	inventory_service = eazel_inventory_get ();

	if (inventory_service) {
		eazel_inventory_upload (inventory_service, 
				inventory_service_callback, rpm_view);
	}
#else
	tmp = g_strdup (nautilus_rpm_view_get_uri (rpm_view));
	nautilus_rpm_view_load_uri (rpm_view, tmp);
	g_free (tmp);
#endif
}

/* BEGIN code chunk from nautilus-install-view.c */

/* signal callback -- ask the user for the root password (for installs) */
static char *
nautilus_service_need_password (GtkObject *object, const char *prompt, 
				NautilusRPMView *view)
{
	char *message = NULL;
	GtkWidget *dialog;
	gboolean okay;
	char *out;

	if (view->details->remembered_password) {
		return g_strdup (view->details->remembered_password);
	}

	if (view->details->password_attempts > 0) {
		message = _("Incorrect password.");
	}

	dialog = nautilus_password_dialog_new ("Authenticate Me", message, prompt, "", TRUE);
	okay = nautilus_password_dialog_run_and_block (NAUTILUS_PASSWORD_DIALOG (dialog));

	if (! okay) {
		/* cancel */
		view->details->password_attempts = 0;
		out = g_strdup ("");
	} else {
		out = nautilus_password_dialog_get_password (NAUTILUS_PASSWORD_DIALOG (dialog));
		if (nautilus_password_dialog_get_remember (NAUTILUS_PASSWORD_DIALOG (dialog))) {
			view->details->remembered_password = g_strdup (out);
		}
	}

	gtk_widget_destroy (dialog);
	gtk_main_iteration ();

	if (okay) {
		view->details->password_attempts++;
	}

	return out;
}

/* bad password -- let em try again? */
static gboolean
nautilus_service_try_again (GtkObject *object, 
			    NautilusRPMView*view)
{
	if (view->details->password_attempts == 0) {
		/* user hit "cancel" */
		return FALSE;
	}

	/* a wrong password shouldn't be remembered :) */
	g_free (view->details->remembered_password);
	view->details->remembered_password = NULL;

	if (view->details->password_attempts >= 3) {
		/* give up. */
		view->details->password_attempts = 0;
		return FALSE;
	}
	return TRUE;
}

static TrilobiteRootClient *
set_root_client (BonoboObjectClient *service, 
		 NautilusRPMView *view)
{
	TrilobiteRootClient *root_client = NULL;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	if (bonobo_object_client_has_interface (service, "IDL:Trilobite/PasswordQuery:1.0", &ev)) {
		root_client = trilobite_root_client_new ();
		if (! trilobite_root_client_attach (root_client, service)) {
			g_warning ("unable to attach root client to Trilobite/PasswordQuery!");
		}

		gtk_signal_connect (GTK_OBJECT (root_client), "need_password",
				    GTK_SIGNAL_FUNC (nautilus_service_need_password),
				    view);
		gtk_signal_connect (GTK_OBJECT (root_client), "try_again",
				    GTK_SIGNAL_FUNC (nautilus_service_try_again),
				    view);
	} else {
		g_warning ("Object does not support IDL:Trilobite/PasswordQuery:1.0");
	}

	CORBA_exception_free (&ev);
	return root_client;
}

/* END code chunk from nautilus-install-view.c */

static gboolean
delete_files (EazelInstallCallback *service,
	      gpointer unused)
{
	return FALSE;
}

/* we don't really need this confirmation stage */
static gboolean
preflight_check (EazelInstallCallback *cb, 
		 EazelInstallCallbackOperation op,
		 const GList *packages,
		 int total_bytes, 
		 int total_packages, 
		 void *unused)
{
	return TRUE;
}

/* and screw transactions */
static gboolean
save_transaction (EazelInstallCallback *cb, 
		  EazelInstallCallbackOperation op,
		  const GList *packages, 
		  void *unused)
{
	return FALSE;
}

static void
nautilus_rpm_view_set_server (NautilusRPMView *rpm_view,
			      EazelInstallCallback *cb,
			      CORBA_Environment *ev)
{
	int port;
	char *host, *p;

	/* get default host/port */
	host = g_strdup (trilobite_get_services_address ());
	if ((p = strchr (host, ':')) != NULL) {
		*p = 0;
	}
	/* always go for the no auth port */
	port = 80;

	GNOME_Trilobite_Eazel_Install__set_server (eazel_install_callback_corba_objref (cb), host, ev);
	GNOME_Trilobite_Eazel_Install__set_server_port (eazel_install_callback_corba_objref (cb), port, ev);

	/* For now always set auth to FALSE, so users are not required to 
	   login to services to install local rpm files */
	GNOME_Trilobite_Eazel_Install__set_auth (eazel_install_callback_corba_objref (cb), FALSE, ev);
}

void 
nautilus_rpm_view_install_package_callback (GtkWidget *widget,
                                            NautilusRPMView *rpm_view)
{
	GList *categories;
	CategoryData *category;
	CORBA_Environment ev;
	EazelInstallCallback *cb;
	EazelInstallProblem *problem = NULL;
 
	CORBA_exception_init (&ev);

	categories = NULL;

	gtk_widget_set_sensitive (GTK_WIDGET (rpm_view->details->package_install_button), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (rpm_view->details->package_uninstall_button), FALSE);
	nautilus_view_report_load_underway (nautilus_rpm_view_get_view (rpm_view));

	g_assert (rpm_view->details->package);
	category = categorydata_new ();
	category->packages = g_list_prepend (NULL, rpm_view->details->package);
	categories = g_list_prepend (NULL, category);

	cb = eazel_install_callback_new ();
	problem = eazel_install_problem_new (); 
	gtk_object_set_data (GTK_OBJECT (cb), "problem-handler", problem);
	
	rpm_view->details->installer = cb;
	rpm_view->details->root_client = set_root_client (eazel_install_callback_bonobo (cb), rpm_view);
	
	GNOME_Trilobite_Eazel_Install__set_protocol (eazel_install_callback_corba_objref (cb), GNOME_Trilobite_Eazel_PROTOCOL_HTTP, &ev);
	nautilus_rpm_view_set_server (rpm_view, cb, &ev);

	gtk_signal_connect (GTK_OBJECT (cb), "download_progress", nautilus_rpm_view_download_progress_signal, rpm_view);
	gtk_signal_connect (GTK_OBJECT (cb), "install_progress", nautilus_rpm_view_install_progress_signal, rpm_view);
	gtk_signal_connect (GTK_OBJECT (cb), "dependency_check", nautilus_rpm_view_dependency_check, rpm_view);
	gtk_signal_connect (GTK_OBJECT (cb), "install_failed", nautilus_rpm_view_install_failed, rpm_view);
	gtk_signal_connect (GTK_OBJECT (cb), "uninstall_failed", nautilus_rpm_view_install_failed, rpm_view);
	gtk_signal_connect (GTK_OBJECT (cb), "download_failed", nautilus_rpm_view_download_failed, rpm_view);
	gtk_signal_connect (GTK_OBJECT (cb), "done", nautilus_rpm_view_install_done, rpm_view);
	gtk_signal_connect (GTK_OBJECT (cb), "preflight_check", GTK_SIGNAL_FUNC (preflight_check), NULL);
	gtk_signal_connect (GTK_OBJECT (cb), "save_transaction", GTK_SIGNAL_FUNC (save_transaction), NULL);

	eazel_install_callback_install_packages (cb, categories, NULL, &ev);

	/* Leak the categories here */

	CORBA_exception_free (&ev);               
}

void 
nautilus_rpm_view_uninstall_package_callback (GtkWidget *widget,
					      NautilusRPMView *rpm_view)
{
	CategoryData *category;
	GList *categories;
	CORBA_Environment ev;
	EazelInstallCallback *cb;		
	EazelInstallProblem *problem = NULL;

	CORBA_exception_init (&ev);

	categories = NULL;

	gtk_widget_set_sensitive (GTK_WIDGET (rpm_view->details->package_install_button), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (rpm_view->details->package_uninstall_button), FALSE);
	nautilus_view_report_load_underway (nautilus_rpm_view_get_view (rpm_view));

	g_assert (rpm_view->details->package);
	category = categorydata_new ();
	category->packages = g_list_prepend (NULL, rpm_view->details->package);
	categories = g_list_prepend (NULL, category);

	cb = eazel_install_callback_new ();
	problem = eazel_install_problem_new (); 
	gtk_object_set_data (GTK_OBJECT (cb), "problem-handler", problem);

	rpm_view->details->installer = cb;
	rpm_view->details->root_client = set_root_client (eazel_install_callback_bonobo (cb), rpm_view);
	
	GNOME_Trilobite_Eazel_Install__set_protocol (eazel_install_callback_corba_objref (cb), GNOME_Trilobite_Eazel_PROTOCOL_HTTP, &ev);
	GNOME_Trilobite_Eazel_Install__set_tmp_dir (eazel_install_callback_corba_objref (cb), "/tmp/eazel-install", &ev);
	gtk_signal_connect (GTK_OBJECT (cb), "download_progress", nautilus_rpm_view_download_progress_signal, rpm_view);
	gtk_signal_connect (GTK_OBJECT (cb), "uninstall_progress", nautilus_rpm_view_install_progress_signal, rpm_view);
	gtk_signal_connect (GTK_OBJECT (cb), "uninstall_failed", nautilus_rpm_view_uninstall_failed, rpm_view);
	gtk_signal_connect (GTK_OBJECT (cb), "dependency_check", nautilus_rpm_view_dependency_check, rpm_view);
	gtk_signal_connect (GTK_OBJECT (cb), "done", nautilus_rpm_view_install_done, rpm_view);
	gtk_signal_connect (GTK_OBJECT (cb), "delete_files", GTK_SIGNAL_FUNC (delete_files), NULL);
	gtk_signal_connect (GTK_OBJECT (cb), "preflight_check", GTK_SIGNAL_FUNC (preflight_check), NULL);
	gtk_signal_connect (GTK_OBJECT (cb), "save_transaction", GTK_SIGNAL_FUNC (save_transaction), NULL);
	
	eazel_install_callback_uninstall_packages (cb, categories, NULL, &ev);
	/* Leak the categories here */
	
	CORBA_exception_free (&ev);               
}
