#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include "installer.h"
#include <eazel-install-public.h>
#include <libtrilobite/helixcode-utils.h>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/utsname.h>

#define EAZEL_SERVICES_DIR_HOME "/var/eazel"
#define EAZEL_SERVICES_DIR EAZEL_SERVICES_DIR_HOME "/services"

#define HOSTNAME "testmachine.eazel.com"
#define PORT_NUMBER 80
#define TMP_DIR "/tmp/eazel-install"
#define RPMRC "/usr/lib/rpm/rpmrc"
#define REMOTE_RPM_DIR "/RPMS"

static char *package_list[LAST] = {
	"/package-list.xml",
	"/package-list-stable.xml",
	"/services-only-list.xml",
	"/package-list.xml",
	"/package-uninstall-list.xml",
};

char *failure_info;
int installer_debug;
int installer_test;
int installer_local;

static void 
eazel_install_progress (EazelInstall *service, 
			const PackageData *pack,
			int package_num, int num_packages, 
			int amount, int total,
			int total_size_completed, int total_size, 
			GtkWidget *widget) 
{
	GtkProgressBar *progressbar, *progress_overall;
	GtkLabel *action_label;
	GtkLabel *package_label;

	action_label = gtk_object_get_data (GTK_OBJECT (widget), "action_label");
	package_label = gtk_object_get_data (GTK_OBJECT (widget), "package_label");
	progressbar = gtk_object_get_data (GTK_OBJECT (widget), "progressbar_single");
	progress_overall = gtk_object_get_data (GTK_OBJECT (widget), "progressbar_overall");

	if (amount == 0) {
		gtk_label_set_text (action_label, "Install :");
		gtk_label_set_text (package_label, pack->name);
		gtk_progress_set_format_string (GTK_PROGRESS (progressbar), "%p%% - %v kb/%u kb");
		gtk_progress_configure (GTK_PROGRESS (progressbar), 0, 0, (float)(total/1024));		
	}

	if (installer_debug) {
		float pct;
		pct = ( (total > 0) ? ((float) ((((float) amount) / total) * 100)): 100.0);
		fprintf (stdout, "Install Progress - %s - %d %d %% %f\r", 
			 pack->name?pack->name:"(null)", amount, total, pct);
	}

	gtk_progress_set_value (GTK_PROGRESS (progressbar), (float)(amount/1024));
	gtk_progress_set_value (GTK_PROGRESS (progress_overall), (float)package_num-1);
	g_main_iteration (FALSE);  

	fflush (stdout);
	if (amount == total && installer_debug) {
		fprintf (stdout, "\n");
	}
	
}


static void 
eazel_download_progress (EazelInstall *service, 
			 const char *name,
			 int amount, 
			 int total,
			 GtkWidget *widget) 
{
	GtkProgressBar *progressbar;
	GtkLabel *action_label;
	GtkLabel *package_label;

	action_label = gtk_object_get_data (GTK_OBJECT (widget), "action_label");
	package_label = gtk_object_get_data (GTK_OBJECT (widget), "package_label");
	progressbar = gtk_object_get_data (GTK_OBJECT (widget), "progressbar_single");

	if (amount == 0) {
		gtk_label_set_text (package_label, name);
		gtk_label_set_text (action_label, "Download :");
		gtk_progress_set_format_string (GTK_PROGRESS (progressbar), "%p%% - %v kb/%u kb");
		gtk_progress_configure (GTK_PROGRESS (progressbar), 0, 0, (float)(total/1024));
	}

	if (installer_debug) {
		float pct;
		pct = ( (total > 0) ? ((float) ((((float) amount) / total) * 100)): 100.0);
		fprintf (stdout, "DOWNLOAD Progress - %s - %d %d %% %f\r", 
			 name?name:"(null)", amount, total, pct); 
		fflush (stdout);
	}
	
	gtk_progress_set_value (GTK_PROGRESS (progressbar), amount/1024);

	if (amount != total) {		
		g_main_iteration (FALSE);
		/* gtk_main_iteration (); */
	} else if (amount == total) {		
		/*
		gtk_progress_set_format_string (GTK_OBJECT (progressbar), "done..."); 
		g_main_iteration (FALSE);
		*/
	}
}

static void
install_failed_helper (EazelInstall *service,
		       const PackageData *pd,
		       char *indent,
		       char **str)
{
	GList *iterator;

	if (pd->toplevel) {
		char *tmp;
		tmp = g_strdup_printf ("%s\n***The package %s failed. Here's the dep tree\n", *str, pd->name);
		g_free (*str);
		(*str) = tmp;
	}
	switch (pd->status) {
	case PACKAGE_DEPENDENCY_FAIL: {
		char *tmp;
		tmp = g_strdup_printf ("%s%s-%s failed\n", *str, indent, rpmfilename_from_packagedata (pd));
		g_free (*str);
		(*str) = tmp;
		break;
	}
	case PACKAGE_CANNOT_OPEN: {
		char *tmp;
		tmp = g_strdup_printf ("%s%s-%s NOT FOUND\n", *str, indent, rpmfilename_from_packagedata (pd));
		g_free (*str);
		(*str) = tmp;
		break;		
	}
	case PACKAGE_SOURCE_NOT_SUPPORTED: {
		char *tmp;
		tmp = g_strdup_printf ("%s%s-%s is a source\n", *str, indent, rpmfilename_from_packagedata (pd));
		g_free (*str);
		(*str) = tmp;
		break;
	}
	case PACKAGE_BREAKS_DEPENDENCY: {
		char *tmp;
		tmp = g_strdup_printf ("%s%s-%s breaks\n", *str, indent, rpmfilename_from_packagedata (pd));
		g_free (*str);
		(*str) = tmp;
		break;
	}
	default: {
		char *tmp;
		tmp = g_strdup_printf ("%s%s-%s\n", *str, indent, rpmfilename_from_packagedata (pd));
		g_free (*str);
		(*str) = tmp;
		break;
	}
	}
	for (iterator = pd->soft_depends; iterator; iterator = iterator->next) {			
		PackageData *pack;
		char *indent2;
		indent2 = g_strconcat (indent, iterator->next ? " |" : "  " , NULL);
		pack = (PackageData*)iterator->data;
		install_failed_helper (service, pack, indent2, str);
		g_free (indent2);
	}
	for (iterator = pd->breaks; iterator; iterator = iterator->next) {			
		PackageData *pack;
		char *indent2;
		indent2 = g_strconcat (indent, iterator->next ? " |" : "  " , NULL);
		pack = (PackageData*)iterator->data;
		install_failed_helper (service, pack, indent2, str);
		g_free (indent2);
	}
}


static void
install_failed (EazelInstall *service,
		const PackageData *pd,
		char **output)
{
	if (pd->toplevel == TRUE) {
		install_failed_helper (service, pd, g_strdup (""), output);
	}
}

static void
download_failed (EazelInstall *service,
		 const char *name,
		 char **output)
{
	(*output) = g_strdup_printf ("Download of %s failed", name);
}

static void
eazel_install_preflight (EazelInstall *service,
			 int total_size,
			 int num_packages,
			 GtkWidget *widget)
{
	GtkLabel *action_label;
	GtkLabel *package_label;
	GtkProgressBar *progress_overall;
	char *tmp;

	action_label = gtk_object_get_data (GTK_OBJECT (widget), "action_label");
	package_label = gtk_object_get_data (GTK_OBJECT (widget), "package_label");
	progress_overall = gtk_object_get_data (GTK_OBJECT (widget), "progressbar_overall");

	gtk_progress_set_format_string (GTK_PROGRESS (progress_overall), "%p%% - package %v of %u");
	gtk_progress_configure (GTK_PROGRESS (progress_overall), 0, 0, (float)num_packages);
	gtk_widget_show (GTK_WIDGET (progress_overall));

	tmp = g_strdup_printf ("Preparing RPM, %d packages (%d mb)", num_packages, total_size/(1024*1024));

	if (installer_debug) {
		fprintf (stdout, "PREFLIGHT: %s\n", tmp);
	}

	gtk_label_set_text (action_label, "Install :");
	gtk_label_set_text (package_label, tmp);
	g_main_iteration (FALSE);
	g_main_iteration (FALSE);
}

static void
eazel_install_dep_check (EazelInstall *service,
			 const PackageData *pack,
			 const PackageData *needs,
			 GtkWidget *widget)
{
	GtkLabel *action_label;
	GtkLabel *package_label;
	char *tmp;

	action_label = gtk_object_get_data (GTK_OBJECT (widget), "action_label");
	package_label = gtk_object_get_data (GTK_OBJECT (widget), "package_label");

	tmp = g_strdup_printf ("%s needs %d", pack->name, needs->name);

	if (installer_debug) {
		fprintf (stdout, "DEP CHECK : %s\n", tmp);
	}

	gtk_label_set_text (action_label, "Dep check :");
	gtk_label_set_text (package_label, tmp);

	g_main_iteration (FALSE);
}

static gboolean
eazel_install_delete_files (EazelInstall *service,
			    GtkWidget *widget) 
{
	if (installer_debug) {
		fprintf (stdout, "Deleting rpm's\n");
	}
	return TRUE ;
}

static void
make_dirs ()
{
	int retval;
	if (! g_file_test (EAZEL_SERVICES_DIR, G_FILE_TEST_ISDIR)) {
		if (! g_file_test (EAZEL_SERVICES_DIR_HOME, G_FILE_TEST_ISDIR)) {
			retval = mkdir (EAZEL_SERVICES_DIR_HOME, 0755);		       
			if (retval < 0) {
				if (errno != EEXIST) {
					g_error (_("*** Could not create services directory (%s)! ***\n"), EAZEL_SERVICES_DIR_HOME);
				}
			}
		}

		retval = mkdir (EAZEL_SERVICES_DIR, 0755);
		if (retval < 0) {
			if (errno != EEXIST) {
				g_error (_("*** Could not create services directory (%s)! ***\n"), EAZEL_SERVICES_DIR);
			}
		}
	}
}

void installer (GtkWidget *window,
		gint method) 
{
	EazelInstall *service;
	GtkProgressBar *progressbar;
	GtkLabel *package_label;
	GtkLabel *action_label;

	make_dirs ();

	if (method==UPGRADE) {
		gnome_warning_dialog ("We don't do UPGRADE yet");
		return;
	}
#ifndef NAUTILUS_INSTALLER_RELEASE
	if (!installer_test) {
		GnomeDialog *d;
		d = GNOME_DIALOG (gnome_warning_dialog_parented (_("This is a warning, you're running\n"
								   "the installer for real, without \n"
								   "the --test flag... Beware!"),
								 GTK_WINDOW (window)));
		gnome_dialog_run_and_close (d);
	} 
#endif
	
	/* We set force, update and downgrade to true. */
	service = EAZEL_INSTALL (gtk_object_new (TYPE_EAZEL_INSTALL,
						 "verbose", TRUE,
						 "silent", FALSE,
						 "debug", TRUE,
						 "test", installer_test ? TRUE : FALSE, 
						 "force", TRUE,
						 "depend", FALSE,
						 "update", TRUE,
						 "uninstall", method==UNINSTALL ? TRUE : FALSE,
						 "downgrade", TRUE,
						 "protocol", installer_local ? PROTOCOL_LOCAL: PROTOCOL_HTTP,
						 "tmp_dir", TMP_DIR,
						 "rpmrc_file", RPMRC,
						 "server", HOSTNAME,
						 "rpm_storage_path", REMOTE_RPM_DIR,
						 "package_list_storage_path", package_list [ method ],
						 "server_port", PORT_NUMBER,
						 NULL));
	g_assert (service != NULL);

	if (!installer_debug) {
		eazel_install_open_log (service, "/tmp/nautilus-install.log");
	}

	g_assert (service != NULL);

	gtk_signal_connect (GTK_OBJECT (service), "download_progress", 
			    GTK_SIGNAL_FUNC (eazel_download_progress), window);
	gtk_signal_connect (GTK_OBJECT (service), "install_progress", 
			    GTK_SIGNAL_FUNC (eazel_install_progress), window);
	gtk_signal_connect (GTK_OBJECT (service), "preflight_check", 
			    GTK_SIGNAL_FUNC (eazel_install_preflight), window);
	gtk_signal_connect (GTK_OBJECT (service), "dependency_check", 
			    GTK_SIGNAL_FUNC (eazel_install_dep_check), window);
	gtk_signal_connect (GTK_OBJECT (service), "delete_files", 
			    GTK_SIGNAL_FUNC (eazel_install_delete_files), window);
	gtk_signal_connect (GTK_OBJECT (service), "download_failed", 
			    GTK_SIGNAL_FUNC (download_failed), &failure_info); 
	gtk_signal_connect (GTK_OBJECT (service), "install_failed", 
			    GTK_SIGNAL_FUNC (install_failed), &failure_info);

	failure_info = g_new0 (char, 8192);

	switch (method) {
	case FULL_INST:
	case NAUTILUS_ONLY:
	case SERVICES_ONLY:
	case UPGRADE:		
		eazel_install_install_packages (service, NULL);
		break;
	case UNINSTALL:
		eazel_install_uninstall_packages (service, NULL);
		break;
	};

	eazel_install_destroy (GTK_OBJECT (service));

	package_label = gtk_object_get_data (GTK_OBJECT (window), "package_label");
	progressbar = gtk_object_get_data (GTK_OBJECT (window), "progressbar_single");
	gtk_progress_set_format_string (GTK_PROGRESS (progressbar), "done");
	progressbar = gtk_object_get_data (GTK_OBJECT (window), "progressbar_overall");
	gtk_progress_set_format_string (GTK_PROGRESS (progressbar), "done");

	if (strlen (failure_info)>1) {
		gtk_label_set_text (package_label, "Failed");	
		if (installer_debug) {
			fprintf (stdout, "ERROR :\n%s", failure_info);
		}
		gnome_error_dialog_parented (failure_info, GTK_WINDOW (window));
	} else {
		gtk_label_set_text (package_label, "Completed");	
	}
}


/* Dummy functions to make linking work */

const gpointer oaf_popt_options = NULL;
gpointer oaf_init (int argc, char *argv[]) {}
int bonobo_init (gpointer a, gpointer b, gpointer c) {};
