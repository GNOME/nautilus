#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include "installer.h"
#include <eazel-install-public.h>
#include <libtrilobite/helixcode-utils.h>

#define HOSTNAME "vorlon.eazel.com"
#define PORT_NUMBER 80
#define PROTOCOL PROTOCOL_HTTP
#define TMP_DIR "/tmp/eazel-install"
#define RPMRC "/usr/lib/rpm/rpmrc"
#define REMOTE_RPM_DIR "/RPMS"
#define PACKAGE_LIST "package-list.xml"

static char *package_list[LAST] = {
	"/package-list.xml",
	"/nautilus-only-list.xml",
	"/services-only-list.xml",
	"/package-list.xml",
	"/package-uninstall-list.xml",
};

char *failure_info;

static void 
eazel_install_progress (EazelInstall *service, 
			const char *name,
			int amount, 
			int total,
			GtkWidget *widget) 
{
	GtkProgressBar *progressbar;
	GtkLabel *action_label;
	GtkLabel *package_label;
	float pct;
 
	pct = ( (total > 0) ? ((float) ((((float) amount) / total) * 100)): 100.0);
	/* fprintf (stdout, "Install Progress - %s - %d %d %% %f\n", name?name:"(null)", amount, total, pct); */

	action_label = gtk_object_get_data (GTK_OBJECT (widget), "action_label");
	package_label = gtk_object_get_data (GTK_OBJECT (widget), "package_label");
	progressbar = gtk_object_get_data (GTK_OBJECT (widget), "progressbar");

	gtk_label_set_text (action_label, "Install :");
	gtk_label_set_text (package_label, name + strlen (TMP_DIR) + 1);
	gtk_progress_bar_update (progressbar, pct/100);

	fflush (stdout);
	if (amount == total) {
		fprintf (stdout, "\n");
	}
	
	gtk_main_iteration ();
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
	float pct;
 
	pct = ( (total > 0) ? ((float) ((((float) amount) / total) * 100)): 100.0);
	/* fprintf (stdout, "DOWNLOAD Progress - %s - %d %d %% %f\n", name?name:"(null)", amount, total, pct); */

	action_label = gtk_object_get_data (GTK_OBJECT (widget), "action_label");
	package_label = gtk_object_get_data (GTK_OBJECT (widget), "package_label");
	progressbar = gtk_object_get_data (GTK_OBJECT (widget), "progressbar");

	gtk_label_set_text (action_label, "Download :");
	gtk_label_set_text (package_label, name + strlen (TMP_DIR) + 1);
	gtk_progress_bar_update (progressbar, pct/100);

	fflush (stdout);
	if (amount != total) {
		gtk_main_iteration ();		
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

void installer (GtkWidget *window,
		gint method) 
{
	EazelInstall *service;
	GtkProgressBar *progressbar;
	GtkLabel *package_label;

	if (method==UPGRADE) {
		gnome_warning_dialog ("We don't do UPGRADE yet");
		return;
	}

	service = EAZEL_INSTALL (gtk_object_new (TYPE_EAZEL_INSTALL,
						 "verbose", TRUE,
						 "silent", FALSE,
						 "debug", TRUE,
						 "test", FALSE,
						 "force", TRUE,
						 "depend", FALSE,
						 "update", TRUE, //method==UPGRADE ? TRUE : FALSE,
						 "uninstall", method==UNINSTALL ? TRUE : FALSE,
						 "downgrade", FALSE,
						 "protocol", PROTOCOL,
						 "tmp_dir", TMP_DIR,
						 "rpmrc_file", RPMRC,
						 "hostname", HOSTNAME,
						 "rpm_storage_path", REMOTE_RPM_DIR,
						 "package_list_storage_path", package_list [ method ],
						 "package_list", PACKAGE_LIST, 
						 "port_number", PORT_NUMBER,
						 NULL));

	service = eazel_install_new_with_config ("/var/eazel/services/eazel-services-config.xml");
	g_assert (service != NULL);

	eazel_install_set_hostname (service, HOSTNAME);
	eazel_install_set_rpmrc_file (service, RPMRC);
	eazel_install_set_package_list_storage_path (service, "/package-list.xml");
	eazel_install_set_rpm_storage_path (service, REMOTE_RPM_DIR);
	eazel_install_set_tmp_dir (service, TMP_DIR);
	eazel_install_set_port_number (service, PORT_NUMBER);
	eazel_install_set_protocol (service, PROTOCOL);	

	g_assert (service != NULL);

	gtk_signal_connect (GTK_OBJECT (service), "download_progress", eazel_download_progress, window);
	gtk_signal_connect (GTK_OBJECT (service), "install_progress", eazel_install_progress, window);
	/* gtk_signal_connect (GTK_OBJECT (service), "download_failed", download_failed, window); */
	gtk_signal_connect (GTK_OBJECT (service), "install_failed", install_failed, &failure_info);

	failure_info = g_new0 (char, 8192);

	switch (method) {
	case FULL_INST:
	case NAUTILUS_ONLY:
	case SERVICES_ONLY:
	case UPGRADE:
		eazel_install_install_packages (service, NULL);
		break;
	case UNINSTALL:
		eazel_install_uninstall (service);
		break;
	};
	eazel_install_destroy (GTK_OBJECT (service));

	package_label = gtk_object_get_data (GTK_OBJECT (window), "package_label");
	progressbar = gtk_object_get_data (GTK_OBJECT (window), "progressbar");

	gtk_label_set_text (package_label, "Completed :");
	gtk_progress_bar_update (progressbar, 1);

	gnome_error_dialog_parented (failure_info, GTK_WINDOW (window));
}
