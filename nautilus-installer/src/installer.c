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
	if (amount == total) {
		fprintf (stdout, "\n");
	}
	
	gtk_main_iteration ();
}

static void
append_name_to (GtkWidget *widget,
		const char *listname,
		const char *name)
{
	GSList *list;

	list = (GSList*)gtk_object_get_data (GTK_OBJECT (widget), listname);
	list = g_slist_append (list, g_strdup (name));
	gtk_object_set_data (GTK_OBJECT (widget), listname, list);
}

void
download_failed (EazelInstall *service,
		 const char *name,
		 GtkWidget *window)
{
	append_name_to (window, "download_failed_list", name);
}

void
install_failed (EazelInstall *service,
		 const char *name,
		 GtkWidget *window)
{
	append_name_to (window, "install_failed_list", name);
}

static char *
gen_report (GtkWidget *widget,
	    const char *listname,
	    char **text,
	    const char *title)
{
	GSList *list;

	list = gtk_object_get_data (GTK_OBJECT (widget), listname);
	if (list != NULL) {
		GSList *ptr;
		if ((*text)==NULL) {
			(*text) = g_strdup (title);
		} else {
			char *tmp;
			tmp = g_strconcat ((*text), title, NULL);
			g_free ((*text));
			(*text) = tmp;
		}
		ptr = list;
		while (ptr) {
			char *tmp;
			tmp = g_strconcat ((*text), "* ", (char*)ptr->data, "\n", NULL);
			g_free ((*text));
			(*text) = tmp;
			ptr = ptr->next;
		}
	}
}

static void
dump_failure_info (GtkWidget *window) 
{
	char *text;

	text = NULL;
	gen_report (window, "download_failed_list", &text, "Download failed for\n");
	gen_report (window, "install_failed_list", &text, "Install failed for\n");
	gnome_warning_dialog (text);
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

	eazel_install_open_log (service, TMP_DIR "/log");
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
	gtk_signal_connect (GTK_OBJECT (service), "download_failed", download_failed, window);
	gtk_signal_connect (GTK_OBJECT (service), "install_failed", install_failed, window);

	switch (method) {
	case FULL_INST:
	case NAUTILUS_ONLY:
	case SERVICES_ONLY:
	case UPGRADE:
		eazel_install_new_packages (service);
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


	dump_failure_info (window);
}
