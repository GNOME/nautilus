/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000, 2001  Eazel, Inc.
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
 * Authors: Eskil Heyn Olsen  <eskil@eazel.com>
 *          Robey Pointer <robey@eazel.com>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <libtrilobite/trilobite-core-network.h>
#include <eazel-install-xml-package-list.h>
#include <eazel-install-protocols.h>
#include <eazel-install-query.h>
#include <eazel-package-system.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/utsname.h>

#include <nautilus-druid.h>
#include <nautilus-druid-page-eazel.h>

#include "installer.h"
#include "package-tree.h"
#include "callbacks.h"
#include "support.h"
#include "proxy.h"

/* Include the pixmaps */
#include "bootstrap-background.xpm"	/* background for every panel */
#include "error-symbol.xpm"		/* icon to add to error panel */

/* Data argument to get_detailed_errors_foreach.
   Contains the installer and a path in the tree
   leading to the actual package */
typedef struct {
	EazelInstaller *installer;
	GList *path;
} GetErrorsForEachData;

/* this means the services have to keep an insecure version running, that has enough stuff for ppl
 * to install nautilus.
 */
#define HOSTNAME	"services.eazel.com"
#define PORT_NUMBER	80
#define CGI_PATH	"/catalog/find"
#define RPMRC		"/usr/lib/rpm/rpmrc"
#define REMOTE_RPM_DIR	"/RPMS"
#define PACKAGE_LIST	"package-list.xml"
#define TEXT_LIST	"installer-strings"

#define LOGFILE		"eazel-install.log"

static const char untranslated_font_norm_bold[] = N_("-adobe-helvetica-bold-r-normal-*-*-120-*-*-p-*-*-*,*-r-*");
static const char untranslated_font_norm[] = N_("-adobe-helvetica-medium-r-normal-*-*-120-*-*-p-*-*-*,*-r-*");
static const char untranslated_font_title[] = N_("-adobe-helvetica-medium-r-normal-*-24-*-*-*-p-*-*-*,*-r-*");
static const char untranslated_font_little[] = N_("-adobe-helvetica-medium-r-normal-*-10-*-*-*-p-*-*-*,*-r-*");
#define FONT_NORM_BOLD	_(untranslated_font_norm_bold)
#define FONT_NORM	_(untranslated_font_norm)
#define FONT_TITLE	_(untranslated_font_title)
#define FONT_LITTLE	_(untranslated_font_little)

#define CONTENT_X	64
#define CONTENT_Y	63

#define ERROR_SYMBOL_X	67
#define ERROR_SYMBOL_Y  59


static const char untranslated_error_need_to_set_proxy[] =
	N_("I can't reach the Eazel servers.  This could be because the\n"
	   "Eazel servers are down, or more likely, because you need to\n"
	   "use a web proxy to access external web servers, and I couldn't\n"
	   "figure out your proxy configuration.\n\n"
	   "If you know you have a web proxy, you can try again by setting\n"
	   "the environment variable \"http_proxy\" to the URL of your proxy\n"
	   "server, and then restarting Eazel Installer.");
static const char untranslated_wait_label[] =
	N_("Please wait while we download and install the files selected.");
static const char untranslated_wait_label_2[] =
	N_("Now starting the install process.  This will take some time, so\n"
	   "please be patient.");
static const char untranslated_error_label[] =
	N_("The installer was not able to complete the installation of the\n"
	   "selected files.  Here's why:");
static const char untranslated_error_label_2[] =
	N_("Look for possible solutions to this problem at:\n"
	   "        http://www.eazel.com/support/\n"
	   "Once you have resolved the problem, please restart the installer.");

static const char untranslated_error_title[] = N_("An error has occurred");
static const char untranslated_splash_title[] = N_("Welcome to the Eazel Installer!");
static const char untranslated_finished_title[] = N_("Congratulations!");

static const char untranslated_what_to_install_label[] = N_("What would you like to install?");
static const char untranslated_what_to_install_label_single[] = N_("What we'll install...");

static const char untranslated_error_RPM_4_not_supported[] =
	N_("RPM version 4.x is not supported, sorry.");
static const char untranslated_error_non_RPM_based_system[] =
	N_("Sorry, but this preview installer only works for RPM-based\n"
	   "systems.  You will have to download the source yourself.\n"
	   "In the future, we will support other packaging formats.");
static const char untranslated_error_untested_RPM_based_system[] =
	N_("You're running the installer on an untested and unsupported\n"
	   "RPM-based Linux distribution. I'll try anyways, but\n"\
	   "it will most likely not work.");
static const char untranslated_error_untested_RPM_based_system_title[]= N_("Unsupported distribution");
static const char untranslated_error_RedHat_6_only[] =
	N_("Sorry, but this is the installer for RedHat 6.\n" \
	   "You need to download the installer for RedHat 7.");
static const char untranslated_error_RedHat_7_only[] = 
	N_("Sorry, but this is the installer for RedHat 7.\n" \
	   "You need to download the installer for RedHat 6.");

#define ERROR_NEED_TO_SET_PROXY _(untranslated_error_need_to_set_proxy)
#define D_WAIT_LABEL _(untranslated_wait_label)
#define D_WAIT_LABEL_2 _(untranslated_wait_label_2)
#define D_ERROR_LABEL _(untranslated_error_label)
#define D_ERROR_LABEL_2 _(untranslated_error_label_2)
#define D_ERROR_TITLE _(untranslated_error_title)
#define D_SPLASH_TITLE _(untranslated_splash_title)
#define D_FINISHED_TITLE _(untranslated_finished_title)
#define D_WHAT_TO_INSTALL_LABEL _(untranslated_what_to_install_label)
#define D_WHAT_TO_INSTALL_LABEL_SINGLE _(untranslated_what_to_install_label_single)
#define D_ERROR_RPM_4_NOT_SUPPORTED _(untranslated_error_RPM_4_not_supported)
#define D_ERROR_NON_RPM_BASED_SYSTEM _(untranslated_error_non_RPM_based_system)
#define D_ERROR_UNTESTED_RPM_BASED_SYSTEM_TEXT _(untranslated_error_untested_RPM_based_system)
#define D_ERROR_UNTESTED_RPM_BASED_SYSTEM_TITLE _(untranslated_error_untested_RPM_based_system_title)
#define D_ERROR_REDHAT_6_ONLY _(untranslated_error_RedHat_6_only)
#define D_ERROR_REDHAT_7_ONLY _(untranslated_error_RedHat_7_only)

#define NAUTILUS_INSTALLER_RELEASE
#undef THAT_DAMN_CHECKBOX

enum {
	ERROR_RPM_4_NOT_SUPPORTED,
	ERROR_REDHAT_6_ONLY,
	ERROR_REDHAT_7_ONLY,
	ERROR_NON_RPM_BASED_SYSTEM,

	ERROR_UNTESTED_RPM_BASED_SYSTEM_TITLE,
	ERROR_UNTESTED_RPM_BASED_SYSTEM_TEXT,

	WAIT_LABEL,
	WAIT_LABEL_2,
	ERROR_LABEL,
	ERROR_LABEL_2,
	WHAT_TO_INSTALL_LABEL,
	WHAT_TO_INSTALL_LABEL_SINGLE,

	ERROR_TITLE,
	SPLASH_TITLE,
	FINISHED_TITLE,

	LAST_LABEL
} text_labels_enums;

char *text_labels[LAST_LABEL];

int installer_debug = 0;
int installer_spam = 0;		/* dump logging stuff to stderr (automatically adds --debug) */
int installer_test = 0;
int installer_force = 0;
int installer_local = 0;
char *installer_package = NULL;
int installer_dont_ask_questions = 0;
char *installer_server =NULL;
int installer_server_port = 0;
char *installer_cgi_path = NULL;
char *installer_tmpdir = "/tmp";
char *installer_homedir = NULL;
char *installer_cache_dir = NULL;

static void check_if_next_okay (GnomeDruidPage *page, void *unused, EazelInstaller *installer);
static void jump_to_retry_page (EazelInstaller *installer);
static void jump_to_error_page (EazelInstaller *installer, GList *bullets, char *text, char *text2);
static GtkObjectClass *eazel_installer_parent_class;


static void
start_over (EazelInstaller *installer)
{
	GtkWidget *install_page;
	g_message ("--- installation round begins ---");
	install_page = gtk_object_get_data (GTK_OBJECT (installer->window), "install_page");
	gnome_druid_set_page (installer->druid, GNOME_DRUID_PAGE (install_page));
}

static gboolean
start_over_callback_druid (GnomeDruidPage *druid_page, 
			   GnomeDruid *druid,
			   EazelInstaller *installer)
{
	start_over (installer);
	return TRUE;	/* yes, i handled the page change */
}

static GtkWidget*
create_what_to_do_page (EazelInstaller *installer)
{
	GtkWidget *what_to_do_page;
	GtkWidget *vbox;
	GtkWidget *title;
	GtkWidget *hbox;

	what_to_do_page = nautilus_druid_page_eazel_new_with_vals (NAUTILUS_DRUID_PAGE_EAZEL_OTHER, 
								   "", "", NULL, NULL,
								   create_pixmap (GTK_WIDGET (installer->window),
										  bootstrap_background));

	gtk_widget_set_name (what_to_do_page, "what_to_do_page");
	gtk_widget_ref (what_to_do_page);
	gtk_object_set_data_full (GTK_OBJECT (installer->window), "what_to_do_page", what_to_do_page,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show_all (what_to_do_page);
	gnome_druid_append_page (GNOME_DRUID (installer->druid), GNOME_DRUID_PAGE (what_to_do_page));

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_ref (vbox);
	gtk_object_set_data_full (GTK_OBJECT (installer->window), "vbox3", vbox, (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (vbox);
	gtk_widget_set_uposition (vbox, CONTENT_X, CONTENT_Y);

	title = gtk_label_new_with_font (text_labels [WHAT_TO_INSTALL_LABEL], FONT_TITLE);
	gtk_label_set_justify (GTK_LABEL (title), GTK_JUSTIFY_LEFT);
	gtk_widget_show (title);
	gtk_widget_ref (title);
	gtk_object_set_data_full (GTK_OBJECT (installer->window), "humleridderne", title,
				  (GtkDestroyNotify) gtk_widget_unref);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), title, FALSE, FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	nautilus_druid_page_eazel_put_widget (NAUTILUS_DRUID_PAGE_EAZEL (what_to_do_page), vbox);

	gtk_signal_connect (GTK_OBJECT (what_to_do_page), "next",
			    GTK_SIGNAL_FUNC (start_over_callback_druid),
			    installer);
	gtk_signal_connect (GTK_OBJECT (what_to_do_page), "prepare",
			    GTK_SIGNAL_FUNC (check_if_next_okay),
			    installer);

	return what_to_do_page;
}

static GtkWidget*
create_install_page (EazelInstaller *installer)
{
	GtkWidget *install_page;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *title;
	GtkWidget *progressbar1;
	GtkWidget *progressbar2;
	GtkWidget *label_single;
	GtkWidget *label_single_2;
	GtkWidget *label_overall;
	GtkWidget *wait_label;
	GtkWidget *download_label;
	GtkWidget *install_label;
	
	install_page = nautilus_druid_page_eazel_new_with_vals (NAUTILUS_DRUID_PAGE_EAZEL_OTHER, 
								"",
								NULL,
								NULL,
								NULL,
								create_pixmap (GTK_WIDGET (installer->window),
									       bootstrap_background));
	gtk_widget_set_name (install_page, "install_page");
	gtk_widget_ref (install_page);
	gtk_object_set_data_full (GTK_OBJECT (installer->window), "install_page", install_page,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show_all (install_page);
	gnome_druid_append_page (GNOME_DRUID (installer->druid), GNOME_DRUID_PAGE (install_page));

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_set_name (vbox, "install/vbox");
	gtk_widget_set_uposition (vbox, CONTENT_X, CONTENT_Y);
	gtk_widget_ref (vbox);
	gtk_object_set_data_full (GTK_OBJECT (installer->window), "vbox", vbox,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (vbox);
	nautilus_druid_page_eazel_put_widget (NAUTILUS_DRUID_PAGE_EAZEL (install_page),
					      vbox);

	title = gtk_label_new_with_font (_("Downloading & Installing..."), FONT_TITLE);
	gtk_label_set_justify (GTK_LABEL (title), GTK_JUSTIFY_LEFT);
	gtk_widget_show (title);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), title, FALSE, FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	wait_label = gtk_label_new (text_labels [WAIT_LABEL]);
	gtk_widget_set_name (wait_label, "label_top");
	gtk_label_set_justify (GTK_LABEL (wait_label), GTK_JUSTIFY_LEFT);
	gtk_widget_ref (wait_label);
	gtk_object_set_data_full (GTK_OBJECT (installer->window), "label_top", wait_label,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (wait_label);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), wait_label, FALSE, FALSE, 30);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 15);

	download_label = gtk_label_new_with_font (_("Download Progress:"), FONT_NORM_BOLD);
	gtk_widget_set_name (download_label, "header_single");
	gtk_label_set_justify (GTK_LABEL (download_label), GTK_JUSTIFY_LEFT);
	gtk_widget_ref (download_label);
	gtk_object_set_data_full (GTK_OBJECT (installer->window), "header_single", download_label,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (download_label);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), download_label, FALSE, FALSE, 40);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	progressbar1 = gtk_progress_bar_new ();
	gtk_widget_set_name (progressbar1, "progressbar_single");
	gtk_widget_ref (progressbar1);
	gtk_object_set_data_full (GTK_OBJECT (installer->window), "progressbar_single", progressbar1,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (progressbar1);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), progressbar1, FALSE, FALSE, 50);
	gtk_widget_set_usize (progressbar1, 300, 20);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox,  FALSE, FALSE, 3);

	label_single = gtk_label_new (_("Contacting the install server..."));
	gtk_widget_set_name (label_single, "download_label");
	gtk_label_set_justify (GTK_LABEL (label_single), GTK_JUSTIFY_LEFT);
	gtk_widget_ref (label_single);
	gtk_object_set_data_full (GTK_OBJECT (installer->window), "download_label", label_single,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label_single);

	label_single_2 = gtk_label_new ("");
	gtk_widget_set_name (label_single_2, "download_label_2");
	gtk_label_set_justify (GTK_LABEL (label_single_2), GTK_JUSTIFY_LEFT);
	gtk_widget_ref (label_single_2);
	gtk_object_set_data_full (GTK_OBJECT (installer->window), "download_label_2", label_single_2,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label_single_2);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_set_name (hbox, "hbox_label_single");
	gtk_widget_ref (hbox);
	gtk_object_set_data_full (GTK_OBJECT (installer->window), "hbox_label_single", label_single,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_box_add_padding (hbox, 50, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label_single, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label_single_2, FALSE, FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox,  FALSE, FALSE, 0);

	gtk_box_add_padding (vbox, 0, 15);

	install_label = gtk_label_new_with_font (_("Overall Progress:"), FONT_NORM_BOLD);
	gtk_label_set_justify (GTK_LABEL (install_label), GTK_JUSTIFY_LEFT);
	gtk_widget_show (install_label);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), install_label, FALSE, FALSE, 40);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	progressbar2 = gtk_progress_bar_new ();
	gtk_widget_set_name (progressbar2, "progressbar_overall");
	gtk_widget_ref (progressbar2);
	gtk_object_set_data_full (GTK_OBJECT (installer->window), "progressbar_overall", progressbar2,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (progressbar2);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), progressbar2, FALSE, FALSE, 50);
	gtk_widget_set_usize (progressbar2, 300, 20);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox,  FALSE, FALSE, 3);

#if 0
	label_overall = gtk_label_new (_("Downloading packages required to install Nautilus"));
#else
	label_overall = gtk_label_new (" ");
#endif
	gtk_widget_set_name (label_overall, "label_overall");
	gtk_label_set_justify (GTK_LABEL (label_overall), GTK_JUSTIFY_LEFT);
	gtk_widget_ref (label_overall);
	gtk_object_set_data_full (GTK_OBJECT (installer->window), "label_overall", label_overall,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label_overall);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label_overall, FALSE, FALSE, 50);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox,  FALSE, FALSE, 0);

	gtk_signal_connect (GTK_OBJECT (install_page), "finish",
			    GTK_SIGNAL_FUNC (druid_finish),
			    installer);
	gtk_signal_connect (GTK_OBJECT (install_page), "prepare",
			    GTK_SIGNAL_FUNC (prep_install),
			    installer);

	return install_page;
}

static GtkWidget *
create_pixmap_widget (GtkWidget *widget, char **xpmdata)
{
	GdkColormap *colormap;
	GdkPixmap *gdkpixmap;
	GdkBitmap *mask;
	GtkWidget *my_widget;

	colormap = gtk_widget_get_colormap (widget);
	gdkpixmap = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &mask, NULL, (gchar **)xpmdata);
	g_assert (gdkpixmap != NULL);
	my_widget = gtk_pixmap_new (gdkpixmap, mask);

	gdk_pixmap_unref (gdkpixmap);
	if (mask != NULL) {
		gdk_bitmap_unref (mask);
	}
	return my_widget;
}

/* adds a bullet point, in boldface, to a vbox: the bullet point should word-wrap correctly */
static void
add_bullet_point_to_vbox (GtkWidget *vbox, const char *text)
{
	GtkWidget *hbox;
	GtkWidget *inner_vbox;
	GtkWidget *label;
	GtkWidget *bullet_label;

	log_debug ("bullet = \"%s\"", text);

	label = gtk_label_new_with_font (text, FONT_NORM_BOLD);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_widget_show (label);
	bullet_label = gtk_label_new_with_font ("\xB7 ", FONT_NORM_BOLD);
	gtk_label_set_justify (GTK_LABEL (bullet_label), GTK_JUSTIFY_LEFT);
	gtk_widget_show (bullet_label);

	/* put the bullet in a vbox so it'll anchor at the top */
	inner_vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (inner_vbox), bullet_label, FALSE, FALSE, 0);
	gtk_widget_show (inner_vbox);

	/* put the anchored bullet and the explanation into an hbox */
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_add_padding (hbox, 45, 0);
	gtk_box_pack_start (GTK_BOX (hbox), inner_vbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
}

static void
jump_to_error_page (EazelInstaller *installer, GList *bullets, char *text, char *text2)
{
	GtkWidget *error_page;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *pixmap;
	GtkWidget *title;
	GtkWidget *label;
	GList *iter;

	error_page = nautilus_druid_page_eazel_new_with_vals (NAUTILUS_DRUID_PAGE_EAZEL_FINISH,
							      "", "", NULL, NULL,
							      create_pixmap (GTK_WIDGET (installer->window),
									     bootstrap_background));
	gtk_widget_show (error_page);
	gnome_druid_append_page (GNOME_DRUID (installer->druid), GNOME_DRUID_PAGE (error_page));

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_set_uposition (vbox, ERROR_SYMBOL_X, ERROR_SYMBOL_Y);
	gtk_widget_show (vbox);
	nautilus_druid_page_eazel_put_widget (NAUTILUS_DRUID_PAGE_EAZEL (error_page), vbox);

	title = gtk_label_new_with_font (text_labels [ERROR_TITLE], FONT_TITLE);
	gtk_label_set_justify (GTK_LABEL (title), GTK_JUSTIFY_LEFT);
	gtk_widget_show (title);
	pixmap = create_pixmap_widget (error_page, error_symbol);
	gtk_widget_show (pixmap);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), pixmap, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), title, FALSE, FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	gtk_box_add_padding (vbox, 0, 20);

	label = gtk_label_new (text);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (label), FALSE);
	gtk_widget_show (label);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 30);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	gtk_box_add_padding (vbox, 0, 15);

	for (iter = g_list_first (bullets); iter != NULL; iter = g_list_next (iter)) {
		add_bullet_point_to_vbox (vbox, (char *)(iter->data));
	}

	gtk_box_add_padding (vbox, 0, 15);

	label = gtk_label_new (text2);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (label), FALSE);
	gtk_widget_show (label);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 30);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	gtk_signal_connect (GTK_OBJECT (error_page), "prepare",
			    GTK_SIGNAL_FUNC (prep_finish),
			    installer);
	gtk_signal_connect (GTK_OBJECT (error_page), "finish",
			    GTK_SIGNAL_FUNC (druid_finish),
			    installer);
	gnome_druid_set_page (installer->druid, GNOME_DRUID_PAGE (error_page));
}

static void
insert_info_page (EazelInstaller *installer,
	  char *title_text,
		  char *info_text)
{
	GtkWidget *info_page;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *pixmap;
	GtkWidget *title;
	GtkWidget *label;

	info_page = nautilus_druid_page_eazel_new_with_vals (NAUTILUS_DRUID_PAGE_EAZEL_OTHER,
							    NULL,
							    NULL, 
							    NULL,
							    NULL,
							    create_pixmap (GTK_WIDGET (installer->window),
									   bootstrap_background));


	gtk_widget_show (info_page);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_set_uposition (vbox, ERROR_SYMBOL_X, ERROR_SYMBOL_Y);
	gtk_widget_show (vbox);
	nautilus_druid_page_eazel_put_widget (NAUTILUS_DRUID_PAGE_EAZEL (info_page), vbox);

	title = gtk_label_new_with_font (title_text, FONT_TITLE);
	gtk_label_set_justify (GTK_LABEL (title), GTK_JUSTIFY_LEFT);
	gtk_widget_show (title);
	pixmap = create_pixmap_widget (info_page, error_symbol);
	gtk_widget_show (pixmap);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), pixmap, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), title, FALSE, FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	gtk_box_add_padding (vbox, 0, 20);

	label = gtk_label_new (info_text);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (label), FALSE);
	gtk_widget_show (label);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 30);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	gnome_druid_insert_page (installer->druid, 
				 installer->back_page,
				 GNOME_DRUID_PAGE (info_page));
	installer->back_page = GNOME_DRUID_PAGE (info_page);
							    
}

static void
skip_over_remove_problems (GtkWidget *widget,
			   EazelInstaller *installer) 
{
	GList *tmp;
	gboolean foo = TRUE;
	EazelInstallProblemEnum p;

	g_message ("in skip_over_remove_problems");
	while (foo) {
		p = eazel_install_problem_find_dominant_problem_type (installer->problem,
								      installer->problems);
		switch (p) {
		case EI_PROBLEM_REMOVE:
		case EI_PROBLEM_FORCE_REMOVE:
		case EI_PROBLEM_CASCADE_REMOVE:
			g_message ("another remove, skipping");
			tmp = eazel_install_problem_step_problem (installer->problem,
								  p,
								  installer->problems);
			g_list_free (installer->problems);
			installer->problems = tmp;
			break;
		default:
			foo = FALSE;
			break;
		}
	}
	/* jump_to_retry_page (installer); */
}

/* give the user an opportunity to retry the install, with new info */
static void
jump_to_retry_page (EazelInstaller *installer)
{
	EazelInstallProblemEnum p;

	p = eazel_install_problem_find_dominant_problem_type (installer->problem,
							      installer->problems);
	
	installer->uninstalling = FALSE;
	if (p == EI_PROBLEM_REMOVE) {
		installer->uninstalling = TRUE;
	} else
	if (p == EI_PROBLEM_FORCE_REMOVE) {
		installer->uninstalling = TRUE;
	} else 
	if (p == EI_PROBLEM_CASCADE_REMOVE) {
		installer->uninstalling = TRUE;
	}

	g_message ("in jump_to_retry_page");
	if (installer->uninstalling) {
		g_message ("uninstalled is set");	
		skip_over_remove_problems (NULL, installer);
		installer->uninstalling = FALSE;
	}
}

static GtkWidget*
create_finish_page_good (GtkWidget *druid, 
			 GtkWidget *window,
			 char *text)
{
	GtkWidget *finish_page;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *title;
	GtkWidget *label;

	finish_page = nautilus_druid_page_eazel_new_with_vals (NAUTILUS_DRUID_PAGE_EAZEL_FINISH,
							       "", "", NULL, NULL,
							       create_pixmap (GTK_WIDGET (window), bootstrap_background));
	gtk_widget_set_name (finish_page, "finish_page_good");
	gtk_widget_ref (finish_page);
	gtk_object_set_data_full (GTK_OBJECT (window), "finish_page_good", finish_page,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (finish_page);
	gnome_druid_append_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (finish_page));

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_set_uposition (vbox, CONTENT_X, CONTENT_Y);
	gtk_widget_show (vbox);
	nautilus_druid_page_eazel_put_widget (NAUTILUS_DRUID_PAGE_EAZEL (finish_page), vbox);

	title = gtk_label_new_with_font (text_labels [FINISHED_TITLE], FONT_TITLE);
	gtk_label_set_justify (GTK_LABEL (title), GTK_JUSTIFY_LEFT);
	gtk_widget_show (title);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), title, FALSE, FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	gtk_box_add_padding (vbox, 0, 20);

	label = gtk_label_new (text);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (label), FALSE);
	gtk_widget_show (label);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 30);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	return finish_page;
}

static GtkWidget*
create_window (EazelInstaller *installer)
{
	GtkWidget *window;
	GtkWidget *druid;
	GtkWidget *start_page;
	char *window_title;
	int x, y;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);	
	gtk_widget_set_name (window, "window");
	gtk_object_set_data (GTK_OBJECT (window), "window", window);
	window_title = g_strdup_printf ("%s - (build %s)", _("Eazel Installer"), BUILD_DATE);
	gtk_window_set_title (GTK_WINDOW (window), window_title);
	g_free (window_title);
	gtk_window_set_policy (GTK_WINDOW (window), FALSE, FALSE, TRUE);
	get_pixmap_width_height (bootstrap_background, &x, &y);
	gtk_widget_set_usize (window, x, y+45);

	druid = nautilus_druid_new ();
	gtk_widget_set_name (druid, "druid");
	gtk_widget_ref (druid);
	gtk_object_set_data_full (GTK_OBJECT (window), "druid", druid,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (druid);
	gtk_container_add (GTK_CONTAINER (window), druid);
	installer->druid = GNOME_DRUID (druid);

	start_page = nautilus_druid_page_eazel_new_with_vals (NAUTILUS_DRUID_PAGE_EAZEL_START,
							      "",
							      _("\n\n\n\n\n          Connecting to Eazel servers ..."),
							      NULL,
							      NULL,
							      create_pixmap (GTK_WIDGET (window), bootstrap_background));

	installer->back_page = GNOME_DRUID_PAGE (start_page);

	gtk_widget_set_name (start_page, "start_page");
	gtk_widget_ref (start_page);
	gtk_object_set_data_full (GTK_OBJECT (window), "start_page", start_page,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (start_page);
	gnome_druid_append_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (start_page));
	gnome_druid_set_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (start_page));

	gtk_signal_connect (GTK_OBJECT (druid), "cancel",
			    GTK_SIGNAL_FUNC (druid_cancel),
			    installer);
	gtk_signal_connect (GTK_OBJECT (druid), "destroy",
			    GTK_SIGNAL_FUNC (druid_delete),
			    installer);

	return window;
}

static void 
eazel_install_progress (EazelInstall *service, 
			const PackageData *package,
			int package_num, int num_packages, 
			int amount, int total,
			int total_size_completed, int total_size, 
			EazelInstaller *installer) 
{
	GtkProgressBar *progressbar, *progress_overall;
	GtkWidget *label_overall;
	GtkWidget *label_single;
	GtkWidget *label_single_2;
	char *temp, *name;
	double percent;

	label_single = gtk_object_get_data (GTK_OBJECT (installer->window), "download_label");
	label_single_2 = gtk_object_get_data (GTK_OBJECT (installer->window), "download_label_2");
	label_overall = gtk_object_get_data (GTK_OBJECT (installer->window), "label_overall");
	progressbar = gtk_object_get_data (GTK_OBJECT (installer->window), "progressbar_single");
	progress_overall = gtk_object_get_data (GTK_OBJECT (installer->window), "progressbar_overall");

#if 0
	if (1) {
		struct timeval now;
		char *timestamp;

		gettimeofday (&now, NULL);
		timestamp = g_malloc (40);
		strftime (timestamp, 40, "%d-%b %H:%M:%S", localtime ((time_t *)&now.tv_sec));
		sprintf (timestamp + strlen (timestamp), ".%02ld ", now.tv_usec/10000L);
		log_debug ("%s: progress on %s (%d of %d): %d of %d (total %d of %d)", timestamp, package->name, package_num, num_packages,  amount, total, total_size_completed, total_size);
		g_free (timestamp);
	}
#endif
	if (amount == 0) {
		name = packagedata_get_readable_name (package);
		temp = g_strdup_printf (_("Installing %s"), name);
		g_free (name);
		gtk_label_set_text (GTK_LABEL (label_single), temp);
		g_free (temp);
		gtk_label_set_text (GTK_LABEL (label_single_2), "");

		gtk_progress_configure (GTK_PROGRESS (progressbar), 0.0, 0.0, 100.0);

		g_message ("Installing: %s", package->name);
	}

	percent = (double)((amount * 100.0) / (total ? total : 0.1));
	gtk_progress_set_value (GTK_PROGRESS (progressbar), percent);
	percent = (double)((total_size_completed * 50.0) / (total_size ? total_size : 0.1));
	percent += 50.0;
	gtk_progress_set_value (GTK_PROGRESS (progress_overall), percent);

	temp = g_strdup_printf (_("Installing %d packages (%d MB)"), installer->total_packages, installer->total_mb);
	gtk_label_set_text (GTK_LABEL (label_overall), temp);
	g_free (temp);

#if 0
	/* absolutely cannot do this anymore! */
	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}
#endif
}


static void
conflict_check (EazelInstall *service, const PackageData *package, EazelInstaller *installer)
{
	GtkWidget *label_single;
	char *out;

	label_single = gtk_object_get_data (GTK_OBJECT (installer->window), "download_label");
	out = g_strdup_printf (_("Checking \"%s\" for conflicts"), package->name);
	gtk_label_set_text (GTK_LABEL (label_single), out);
	g_free (out);

	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}
}

static void 
eazel_download_progress (EazelInstall *service, 
			 const PackageData *package,
			 int amount, 
			 int total,
			 EazelInstaller *installer) 
{
	GtkWidget *progress_single;
	GtkWidget *progress_overall;
	GtkWidget *label_single;
	GtkWidget *label_single_2;
	GtkWidget *label_overall;
	char *temp;
	int amount_KB = (amount+512)/1024;
	int total_KB = (total+512)/1024;

	label_overall = gtk_object_get_data (GTK_OBJECT (installer->window), "label_overall");
	label_single = gtk_object_get_data (GTK_OBJECT (installer->window), "download_label");
	label_single_2 = gtk_object_get_data (GTK_OBJECT (installer->window), "download_label_2");
	progress_single = gtk_object_get_data (GTK_OBJECT (installer->window), "progressbar_single");
	progress_overall = gtk_object_get_data (GTK_OBJECT (installer->window), "progressbar_overall");

	if (amount == 0) {
		gtk_progress_configure (GTK_PROGRESS (progress_single), 0, 0, (float)total);
		gtk_progress_configure (GTK_PROGRESS (progress_overall), 0, 0, (float)installer->total_size);
		temp = g_strdup_printf ("Getting package \"%s\"  ", package->name);
		gtk_label_set_text (GTK_LABEL (label_single), temp); 
		g_free (temp);
		installer->last_KB = 0;
		installer->downloaded_anything = TRUE;
	}

	gtk_progress_set_value (GTK_PROGRESS (progress_single), (float)amount);
	gtk_progress_set_value (GTK_PROGRESS (progress_overall), (float)(installer->total_bytes_downloaded + amount));

	if ((amount_KB >= installer->last_KB+10) || ((amount_KB == total_KB) && (total_KB != 0))) {
		temp = g_strdup_printf ("%dK of %dK", amount_KB, total_KB);
		gtk_label_set_text (GTK_LABEL (label_single_2), temp); 
		g_free (temp);
		installer->last_KB = amount_KB;
	}

	if (amount == total) {
		installer->total_bytes_downloaded += total;
	}

	/* for some reason, we have to prod GTK while downloading... */
	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}
}

#if 0
/* used to be used by eazel-hacking force remove */
static void
create_initial_force_remove_category (EazelInstaller *installer) 
{
	CategoryData *cat = categorydata_new ();
	cat->name = g_strdup ("Stuff to remove");
	cat->packages = NULL;
	installer->force_remove_categories = g_list_prepend (NULL, cat);
	installer->uninstalling = TRUE;
}

static void
add_force_remove (EazelInstaller *installer, 
		  PackageData *pack)
{
	CategoryData *cat;
	log_debug ("add_force_remove_package");
	
	if (installer->force_remove_categories == NULL) {
		create_initial_force_remove_category (installer);
	} 
	cat = (CategoryData*)installer->force_remove_categories->data;
	cat->packages = g_list_prepend (cat->packages, pack);
}
#endif

static void get_detailed_errors_foreach (PackageData *pack, GetErrorsForEachData *data);

static void
get_detailed_errors_foreach_dep (PackageDependency *dep, GetErrorsForEachData *data)
{
	get_detailed_errors_foreach (dep->package, data);
}

static void
get_detailed_errors_foreach (PackageData *pack, GetErrorsForEachData *data)
{
	char *message, *distro;
	EazelInstaller *installer = data->installer; 
	PackageData *pack_in;
	CategoryData *cat;
	GList *iter, *iter2;

	if (data->path != NULL) {
		if (g_list_find (data->path, pack) != NULL) {
			/* recursing... */
			return;
		}
	}

	log_debug ("pack->name = %s, pack->status = %d", pack->name, pack->status);

	/* is this the right place for this check anymore? */
	if (pack->status == PACKAGE_CANNOT_OPEN) {
		/* check if the package we could not open was in categories, since
		   then it's a distro issue. Don't use install_categories, as if eg. 
		   gnumeric is added because of need upgrade, but fails for some reason, 
		   people get told that it could be a distro issue. */
		for (iter = installer->categories; iter; iter = g_list_next (iter)) {
			cat = (CategoryData *)iter->data;
			for (iter2 = cat->packages; iter2 ; iter2 = g_list_next (iter2)) {
				pack_in = PACKAGEDATA (iter2->data);
				log_debug ("pack->name = %s, pack_in->name = %s", pack->name, pack_in->name);
				if (strcmp (pack->name, pack_in->name) == 0) {
					g_message ("bad mojo: cannot open package %s", pack->name);
					distro = trilobite_get_distribution_name (trilobite_get_distribution (),
										  TRUE, FALSE);
					message = g_strdup_printf (_("Initial package download failed: Possibly your "
								     "distribution (%s) isn't supported by Eazel yet, "
								     "or the Eazel servers are offline."),
								   distro);
					installer->failure_info = g_list_prepend (installer->failure_info, message);
					g_free (distro);
				}
			}
		}
	}

/*
	if (pack->conflicts_checked && !pack->toplevel) {
		GList *packages;
		CategoryData *cat = (CategoryData*)(installer->install_categories->data);
		g_message ("adding %s to install_categories", required);
		packages = cat->packages;
		packages = g_list_prepend (packages, pack); 		
		cat->packages = packages;
	}
*/

	/* Create the path list */
	data->path = g_list_prepend (data->path, pack);

	g_list_foreach (pack->depends, (GFunc)get_detailed_errors_foreach_dep, data);
	g_list_foreach (pack->modifies, (GFunc)get_detailed_errors_foreach, data);
	g_list_foreach (pack->breaks, (GFunc)get_detailed_errors_foreach, data);

	/* Pop the currect pack from the path */
	data->path = g_list_remove (data->path, pack);
}

static void
get_detailed_errors (const PackageData *pack, EazelInstaller *installer)
{
	GetErrorsForEachData data;
	PackageData *non_const_pack;
	char *name;

	name = packagedata_get_readable_name (pack);
	log_debug ("error tree traversal begins: errant package %s", name);
	g_free (name);

	if (eazel_install_failed_because_of_disk_full (installer->service)) {
		installer->failure_info = g_list_prepend (installer->failure_info,
							  _("You've run out of disk space!"));
	}

	data.installer = installer;
	data.path = NULL;
	log_debug ("copying package");
	non_const_pack = PACKAGEDATA (pack);
	gtk_object_ref (GTK_OBJECT (non_const_pack));
	//	non_const_pack = packagedata_copy (pack, TRUE);
	log_debug ("getting detailed errors");
	get_detailed_errors_foreach (non_const_pack, &data);
	log_debug ("destroying copy");
	gtk_object_unref (GTK_OBJECT (non_const_pack));
}


static void
collect_failure_info (EazelInstall *service,
		      const PackageData *pd,
		      EazelInstaller *installer,
		      gboolean uninstall)
{
	GList *failure_info_addition;

	eazel_install_problem_tree_to_case (installer->problem,
					    pd,
					    uninstall,
					    &(installer->problems));
	if (!installer->failure_info || 1) {
		/* could be multiple toplevel packages */
		failure_info_addition = eazel_install_problem_tree_to_string (installer->problem,
									      pd,
									      uninstall);
		if (installer->failure_info) {
			installer->failure_info = g_list_concat (installer->failure_info, 
								 failure_info_addition);
		} else {
			installer->failure_info = failure_info_addition;
		}
	}

	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}
}

static void
install_failed (EazelInstall *service,
		const PackageData *pd,
		EazelInstaller *installer)
{
	g_message ("INSTALL FAILED.");
	
	get_detailed_errors (pd, installer);
	collect_failure_info (service, pd, installer, FALSE);
}

static void
uninstall_failed (EazelInstall *service,
		  const PackageData *pd,
		  EazelInstaller *installer)
{
	g_message ("UNINSTALL FAILED.");
	collect_failure_info (service, pd, installer, TRUE);
}

static void
download_failed (EazelInstall *service,
		 const PackageData *package,
		 EazelInstaller *installer)
{
	char *temp;

	if (! eazel_install_failed_because_of_disk_full (service)) {
		temp = g_strdup_printf (_("Download of %s failed"), package->name);
		installer->failure_info = g_list_append (installer->failure_info, temp);
	}
	g_message ("Download FAILED for %s", package->name);
}

static gboolean
eazel_install_preflight (EazelInstall *service,
			 const GList *packages,
			 int total_size,
			 int num_packages,
			 EazelInstaller *installer)
{
	GtkProgressBar *progress_overall;
	GtkProgressBar *progress_single;
	GtkWidget *label_single;
	GtkWidget *label_single_2;
	GtkWidget *label_overall;
	GtkWidget *label_top;
	GtkWidget *header_single;
	char *temp;
	int total_mb;

	if (0) {
		jump_to_package_tree_page (installer, (GList *)packages);
		while (1) { while (gtk_events_pending ()) { gtk_main_iteration (); } }
	}

	label_single = gtk_object_get_data (GTK_OBJECT (installer->window), "download_label");
	label_single_2 = gtk_object_get_data (GTK_OBJECT (installer->window), "download_label_2");
	label_overall = gtk_object_get_data (GTK_OBJECT (installer->window), "label_overall");
	label_top = gtk_object_get_data (GTK_OBJECT (installer->window), "label_top");
	progress_single = gtk_object_get_data (GTK_OBJECT (installer->window), "progressbar_single");
	progress_overall = gtk_object_get_data (GTK_OBJECT (installer->window), "progressbar_overall");
	header_single = gtk_object_get_data (GTK_OBJECT (installer->window), "header_single");
	g_assert (label_single != NULL);
	g_assert (label_single_2 != NULL);
	g_assert (label_overall != NULL);
	g_assert (label_top != NULL);
	g_assert (progress_single != NULL);
	g_assert (progress_overall != NULL);
	g_assert (header_single != NULL);

	/* please wait for blah blah. */
	gtk_label_set_text (GTK_LABEL (label_top), text_labels [WAIT_LABEL_2]);

	/* change header from Download to Install */
	gtk_label_set_text (GTK_LABEL (header_single), _("Install Progress:"));
	gtk_label_set_text (GTK_LABEL (label_single), _("Preparing to install Nautilus and its dependencies"));
	gtk_label_set_text (GTK_LABEL (label_single_2), "");

	gtk_progress_set_percentage (GTK_PROGRESS (progress_single), 0.0);

	total_mb = (total_size + (512*1024)) / (1024*1024);
	if (num_packages == 1) {
		if (installer->uninstalling) {
			temp = g_strdup_printf (_("Uninstalling 1 package"));
		} else {
			temp = g_strdup_printf (_("Downloading 1 package (%d MB)"), total_mb);
			gtk_progress_configure (GTK_PROGRESS (progress_overall), 0.0, 0.0, 100.0);
		}
	} else {
		if (installer->uninstalling) {
			temp = g_strdup_printf (_("Uninstalling %d packages"), num_packages);
			gtk_progress_configure (GTK_PROGRESS (progress_overall), 0.0, 0.0, 100.0);
		} else {
			temp = g_strdup_printf (_("Downloading %d packages (%d MB)"), num_packages, total_mb);
			gtk_progress_configure (GTK_PROGRESS (progress_overall), 0.0, 0.0, 100.0);
		}
	}
	gtk_label_set_text (GTK_LABEL (label_overall), temp);
	log_debug ("PREFLIGHT: %s", temp);
	g_free (temp);

	installer->downloaded_anything = TRUE;
	installer->total_packages = num_packages;
	installer->total_size = total_size;
	installer->total_mb = total_mb;

	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}
	
	return TRUE;
}

static void
eazel_install_dep_check (EazelInstall *service,
			 const PackageData *pack,
			 const PackageData *needs,
			 EazelInstaller *installer)
{
	GtkWidget *label_single;
	char *temp;
	char *original = packagedata_get_readable_name (pack);
	char *required = packagedata_get_readable_name (needs);

	label_single = gtk_object_get_data (GTK_OBJECT (installer->window), "download_label");
	/* careful: this needs->name is not always a package name (sometimes it's a filename) */
	temp = g_strdup_printf ("Getting information about %s ...", original);
	gtk_label_set_text (GTK_LABEL (label_single), temp);
	g_free (temp);

	log_debug ("Dependency: %s needs %s", original, required);
	installer->got_dep_check = TRUE;

	g_free (required);
	g_free (original);

	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}
}

static void
install_done (EazelInstall *service,
	      gboolean result,
	      EazelInstaller *installer)
{
	char *temp = NULL;

	installer->successful = result;
	log_debug ("Done, result is %s", result ? "good" : "evil");
	if (result == FALSE) {
		/* will call jump_to_error_page later */
		if (installer->problems == NULL) {
			if (! installer->failure_info) {
				if (installer->got_dep_check) {
					temp = g_strdup (_("The RPM installer gave an unexpected error"));
				} else {
					temp = g_strdup (_("Eazel's servers are temporarily out of service"));
				}
			}
			if (temp) {
				installer->failure_info = g_list_append (installer->failure_info, temp);
			}
		}
	} else if (installer->uninstalling == FALSE) {
		installer->install_categories = FALSE;
		installer->problems = NULL;
	}
}

/* make the "next" button active only if at least one checkbox is set */
static void
check_if_next_okay (GnomeDruidPage *page, void *unused, EazelInstaller *installer)
{
	GList *iter;
	CategoryData *category;
	GtkWidget *button;
	int pressed = 0;

	for (iter = g_list_first (installer->categories); iter != NULL; iter = g_list_next (iter)) {
		category = (CategoryData *)(iter->data);
		button = (GtkWidget *) gtk_object_get_data (GTK_OBJECT (installer->window), category->name);
		if (button == NULL) {
			g_warning ("Invalid button for '%s'!", category->name);
		} else {
			if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button))) {
				pressed++;
			}
		}
	}

	if (pressed == 0) {
		gnome_druid_set_buttons_sensitive (installer->druid, TRUE, FALSE, TRUE);
	} else {
		gnome_druid_set_buttons_sensitive (installer->druid, TRUE, TRUE, TRUE);
	}
}

static int
category_compare_func (const CategoryData *category, const char *name)
{
	return (g_strcasecmp (category->name, name));
}

static void
toggle_button_lock (EazelInstaller *installer, char *name, gboolean lock) 
{
	GtkWidget *button;
	GtkWidget *label;
	char *temp;
	
	button = gtk_object_get_data (GTK_OBJECT (installer->window), name);
	temp = g_strdup_printf ("%s/label", gtk_widget_get_name (GTK_WIDGET (button)));
	label = gtk_object_get_data (GTK_OBJECT (installer->window), name);
	g_free (temp);

	if (button) {
		if (lock) {
			gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);		
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
		} else {
			gtk_widget_set_sensitive (GTK_WIDGET (button), TRUE);		
		}		
	} else {
		g_warning ("Wanted to lock unknown button %s", name);
	}
}

static void
go_live (GtkToggleButton *button,
	 EazelInstaller *installer)
{
	if (gtk_toggle_button_get_active (button)) {
		gnome_druid_set_buttons_sensitive (installer->druid, FALSE, TRUE, TRUE);
	} else {
		gnome_druid_set_buttons_sensitive (installer->druid, FALSE, FALSE, TRUE);
	}
}

static void
toggle_button_toggled (GtkToggleButton *button,
		       EazelInstaller *installer) 
{
	GList *iterator;
	GList *item;
	CategoryData *category, *category2;
	GtkWidget *other_button;

	log_debug ("%s toggled to %s\n", gtk_widget_get_name (GTK_WIDGET (button)),
		   button->active ? "ACTIVE" : "deactivated");

	item = g_list_find_custom (installer->categories, gtk_widget_get_name (GTK_WIDGET (button)),
				   (GCompareFunc)category_compare_func);
	if (item) {
		category = (CategoryData *)(item->data);
		for (iterator = category->depends; iterator; iterator = iterator->next) {
			toggle_button_lock (installer, 
					    (char*)iterator->data,
					    button->active);		
		}
		if (category->exclusive && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button))) {
			for (iterator = installer->categories; iterator; iterator = iterator->next) {
				category2 = (CategoryData *)(iterator->data);
				other_button = gtk_object_get_data (GTK_OBJECT (installer->window), category2->name);
				if (other_button && (category != category2)) {
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (other_button), FALSE);
				}
			}
		}
	}

	check_if_next_okay (NULL, NULL, installer);
}

static void 
eazel_installer_add_category (EazelInstaller *installer,
			      CategoryData *category,
			      gboolean only_one_category)
{
	GtkWidget *button;
	GtkWidget *vbox;
	gboolean render = TRUE;
	GtkWidget *label;
	GtkWidget *button_name;
	GtkWidget *hbox, *hbox2;
	GtkWidget *vbox_desc;
	char *temp;
	char *section;
	char *p, *lastp;

	log_debug ("Read category \"%s\"", category->name);
	if (category->exclusive) {
		log_debug ("it's exclusive.");
	}

	vbox = GTK_WIDGET (gtk_object_get_data (GTK_OBJECT (installer->window), "vbox3"));

	hbox = gtk_hbox_new (FALSE, 0);
	button = gtk_check_button_new ();
	button_name = gtk_label_new_with_font (category->name, FONT_NORM_BOLD);

	if (only_one_category) {
		/* change the heading */
		label = gtk_object_get_data (GTK_OBJECT (installer->window), "humleridderne");
		gtk_label_set_text (GTK_LABEL (label), text_labels [WHAT_TO_INSTALL_LABEL_SINGLE]);
		label = NULL;
	}

	if (! only_one_category) {
		gtk_widget_show (button);
	}
	gtk_widget_show (button_name);
	gtk_box_add_padding (hbox, 10, 0);
	gtk_box_pack_start (GTK_BOX (hbox), button, 0, 0, 0);
	gtk_box_pack_start (GTK_BOX (hbox), button_name, 0, 0, 0);

	gtk_widget_set_name (button, category->name);
	gtk_widget_ref (button);
	gtk_object_set_data_full (GTK_OBJECT (installer->window), category->name, button,
				  (GtkDestroyNotify) gtk_widget_unref);

	temp = g_strdup_printf ("%s/label", category->name);
	gtk_widget_set_name (button_name, temp);
	gtk_widget_ref (button_name);
	gtk_object_set_data_full (GTK_OBJECT (installer->window), temp, button_name,
				  (GtkDestroyNotify) gtk_widget_unref);
	g_free (temp);

	if (category->description == NULL) {
		category->description = g_strdup ("");
	}

	if (category->default_choice || only_one_category) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	}

	vbox_desc = gtk_vbox_new (FALSE, 0);

	/* convert blank lines into something nicer looking
	 * (gtk label makes the blank lines be huuuuge gaps)
	 */
	lastp = category->description;
	while (lastp && *lastp) {
		p = strstr (lastp, "\n\n");
		if (p == NULL) {
			p = category->description + strlen (category->description);
		}
		section = g_strndup (lastp, p - lastp);
		label = gtk_label_new_with_font (section, FONT_LITTLE);
		gtk_label_set_line_wrap (GTK_LABEL (label), FALSE);
		gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
		hbox2 = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, only_one_category ? 20 : 40);
		gtk_widget_show (label);
		g_free (section);

		gtk_widget_show (hbox2);
		gtk_box_pack_start (GTK_BOX (vbox_desc), hbox2, FALSE, FALSE, 0);

		if (*p) {
			lastp = p+2;
			gtk_box_add_padding (vbox_desc, 0, 10);
		} else {
			lastp = p;
		}
	}

	if (g_list_find_custom (installer->must_have_categories, category->name, (GCompareFunc)g_strcasecmp)) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	} else if (g_list_find_custom (installer->implicit_must_have, category->name, 
				       (GCompareFunc)g_strcasecmp)) {
		gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);		
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	} 
	if (g_list_find_custom (installer->dont_show, category->name, (GCompareFunc)g_strcasecmp)) {
		render = FALSE;
	}
	if (render) {
		gtk_widget_show (hbox);
		gtk_widget_show (vbox_desc);
		gtk_box_add_padding (vbox, 0, 10);
		gtk_box_pack_start (GTK_BOX (vbox), hbox, 0, 0, 0);
		gtk_box_add_padding (vbox, 0, 3);
		gtk_box_pack_start (GTK_BOX (vbox), vbox_desc, 0, 0, 0);
	}

	/* We need to add this signal last, to avoid 
	   activating MUST_INSTALL dependencies,
	   which should be handled by check_system */
	gtk_signal_connect (GTK_OBJECT (button), 
			    "toggled", 
			    GTK_SIGNAL_FUNC (toggle_button_toggled),
			    installer);
}

static gboolean
check_system (EazelInstaller *installer)
{
	DistributionInfo dist;
#ifndef NAUTILUS_INSTALLER_RELEASE
	struct utsname ub;
#endif

	dist = trilobite_get_distribution ();

#ifndef NAUTILUS_INSTALLER_RELEASE
	uname (&ub);
	/* This codes tells Eskil that he's an idiot if he runs it on his own machine
	   without the testflag, since it hoses the system.
	   It rouhgly translates into "fuck off". */
	log_debug ("host = %s", ub.nodename);
	if (!installer_test && g_strncasecmp (ub.nodename, "toothgnasher", 12)==0) {
		GnomeDialog *d;

		d = GNOME_DIALOG (gnome_warning_dialog_parented ("Eskil, din pattestive smølf!!\n"
								 "Hvor godt syn's du selv det går? At\n"
								 "udføre denne installation på din egen\n"
								 "maskine er jo faktisk snotdumt.\n"
								 "Jeg slår lige --test til...",
								 GTK_WINDOW (installer->window)));
		installer->test = 1;
		gnome_dialog_run_and_close (d);
	}
	if (!installer_test && g_strncasecmp (ub.nodename, "tortoise", 8) == 0) {
		GnomeDialog *d;

		d = GNOME_DIALOG (gnome_warning_dialog_parented ("Robey, vi'as kaco!  Tajpu --test dum iniciati\n"
								 "en via propra komputero!  Au khaosoj!",
								 GTK_WINDOW (installer->window)));
		installer->test = 1;
		gnome_dialog_run_and_close (d);
	}

	if (!installer->test) {
		GnomeDialog *d;
		d = GNOME_DIALOG (gnome_warning_dialog_parented (_("This is a warning, you're running\n"
								   "the installer for real, without \n"
								   "the --test flag... Beware!"),
								 GTK_WINDOW (installer->window)));
		gnome_dialog_run_and_close (d);
	} 
#endif

	if (dist.name != DISTRO_REDHAT) {
		/* FIXME bugzilla.eazel.com
		   Find other distro's that use rpm */
		if (dist.name == DISTRO_MANDRAKE ||
		    dist.name == DISTRO_YELLOWDOG ||
		    dist.name == DISTRO_SUSE) {
			insert_info_page (installer, 
					  text_labels [ERROR_UNTESTED_RPM_BASED_SYSTEM_TITLE],
					  text_labels [ERROR_UNTESTED_RPM_BASED_SYSTEM_TEXT]);
		} else {
			jump_to_error_page (installer, NULL,
					    text_labels [ERROR_NON_RPM_BASED_SYSTEM], 
					    "");
			return FALSE;
		}
#if RPM_MAJOR == 3
	} else if (dist.version_major == 7) {
		jump_to_error_page (installer, NULL, text_labels [ERROR_REDHAT_6_ONLY], "");
		return FALSE;
#else
#if RPM_MAJOR == 4
	} else if (dist.version_major == 6) {
		jump_to_error_page (installer, NULL, text_labels [ERROR_REDHAT_7_ONLY], "");
		return FALSE;
#else
	} else {
		insert_info_page (installer,
				  text_labels [ERROR_UNTESTED_RPM_BASED_SYSTEM_TITLE],
				  text_labels [ERROR_UNTESTED_RPM_BASED_SYSTEM_TEXT]);
#endif
#endif
	}

	return TRUE;
}

#if 0
void
revert_nautilus_install (EazelInstall *service)
{
	DIR *dirent;
	struct dirent *de;

	dirent = opendir (EAZEL_SERVICES_DIR);
	
	while (de = readdir (dirent)) {
		if (strncmp (de->d_name, "transaction.", 12)==0) {
			eazel_install_revert_transaction_from_file (service, de->d_name);
			unlink (de->d_name);
		}
	}
}
#endif 

/* if there's more to do, it'll jump to a retry page */
void
eazel_installer_do_install (EazelInstaller *installer, 
			    GList *install_categories,
			    gboolean remove)
{
	GList *categories_copy = NULL;

	categories_copy = categorydata_list_copy (install_categories);
	g_list_foreach (installer->failure_info,
			(GFunc)g_free, 
			NULL);
	g_list_free (installer->failure_info);
	installer->failure_info = NULL;	

	if (remove) {
		eazel_install_set_uninstall (installer->service, TRUE);
		eazel_install_set_force (installer->service, TRUE);		
		eazel_install_uninstall_packages (installer->service, categories_copy, NULL);
	} else {
		installer->uninstalling = FALSE;
		eazel_install_set_uninstall (installer->service, FALSE);
		eazel_install_set_force (installer->service, FALSE);		
		eazel_install_set_update (installer->service, TRUE);		
		eazel_install_install_packages (installer->service, categories_copy, NULL);
	}
	/* now free this copy */
	categorydata_list_destroy (categories_copy);
}

void
eazel_installer_post_install (EazelInstaller *installer)
{
       	GList *iter;

	if (installer->failure_info != NULL) {
		if (installer->debug) {
			for (iter = g_list_first (installer->failure_info); iter != NULL; iter = g_list_next (iter)) {
				log_debug ("ERROR : %s", (char *)(iter->data));
			}
		}
	}
	if (installer->problems) {
		jump_to_retry_page (installer);
	} else if (installer->successful == FALSE) {
		jump_to_error_page (installer, installer->failure_info, 
				    text_labels [ERROR_LABEL], 
				    text_labels [ERROR_LABEL_2]);
	} else if (installer->uninstalling == FALSE) {
		installer->install_categories = NULL;
		installer->successful = FALSE;
		gnome_druid_set_page (installer->druid, installer->finish_good); 
	} else if (installer->uninstalling==TRUE && installer->install_categories) {
		/* begin_install (installer); */
	}
	log_debug ("out of post_install");
}

/* fill in the splash text to look nice */
static void
draw_splash_text (EazelInstaller *installer, const char *splash_text)
{
	GtkWidget *title;
	GtkWidget *label;
	GtkWidget *vbox, *hbox1, *hbox2;
	GtkWidget *start_page;
	GtkWidget *button;

	start_page = gtk_object_get_data (GTK_OBJECT (installer->window), "start_page");
	nautilus_druid_page_eazel_set_text (NAUTILUS_DRUID_PAGE_EAZEL (start_page), "");

	/* put it in an hbox so it won't be indirectly centered */
	title = gtk_label_new_with_font (text_labels [SPLASH_TITLE], FONT_TITLE);
	gtk_label_set_justify (GTK_LABEL (title), GTK_JUSTIFY_LEFT);
	gtk_widget_show (title);
	hbox1 = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox1), title, FALSE, FALSE, 0);
	gtk_widget_show (hbox1);

	if (splash_text != NULL) {
		label = gtk_label_new (splash_text);
	} else {
		/* come up with something vaguely acceptable */
		g_warning ("Didn't get splash text!");
		label = gtk_label_new (_("Press \"Next\" to begin..."));
	}
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_widget_show (label);
	hbox2 = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 20);
	gtk_widget_show (hbox2);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox1, FALSE, FALSE, 0);
	gtk_box_add_padding (vbox, 0, 10);
	gtk_box_pack_start (GTK_BOX (vbox), hbox2, FALSE, FALSE, 0);	
	gtk_widget_set_uposition (vbox, CONTENT_X, CONTENT_Y);
	gtk_widget_show (vbox);

	button = gtk_check_button_new_with_label ("I am now ready to install Nautilus.");
#ifdef THAT_DAMN_CHECKBOX
	gtk_widget_show (button);
	gtk_object_ref (GTK_OBJECT (button));
	gtk_object_set_data_full (GTK_OBJECT (installer->window), "kohberg", button,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_box_add_padding (vbox, 0, 10);
	gtk_box_pack_start (GTK_BOX (vbox), button, 0, 0, 0);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", GTK_SIGNAL_FUNC (go_live),
			    installer);
#else
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	go_live (GTK_TOGGLE_BUTTON (button), installer);
	gnome_druid_set_buttons_sensitive (installer->druid, FALSE, TRUE, TRUE);
#endif

	nautilus_druid_page_eazel_put_widget (NAUTILUS_DRUID_PAGE_EAZEL (start_page), vbox);
}


/*****************************************
  GTK+ object stuff
*****************************************/

static void
eazel_installer_finalize (GtkObject *object)
{
	EazelInstaller *installer;

	log_debug ("eazel_installer_finalize");

	g_return_if_fail (object != NULL);
	g_return_if_fail (EAZEL_INSTALLER (object));

	installer = EAZEL_INSTALLER (object);

	/* Free the objects own crap */
	if (installer->failure_info) {
		g_list_foreach (installer->failure_info, (GFunc)g_free, NULL);
		g_list_free (installer->failure_info);
	}
	g_list_foreach (installer->categories, (GFunc)categorydata_destroy_foreach, NULL);
	g_list_free (installer->categories);
#if 0
	if (installer->service != NULL) {
		gtk_object_unref (GTK_OBJECT (installer->service));
	}
#endif
	if (installer->problem != NULL) {
		gtk_object_unref (GTK_OBJECT (installer->problem));
	}
	g_free (installer->tmpdir);

	/* Call parents destroy */
	if (GTK_OBJECT_CLASS (eazel_installer_parent_class)->finalize) {
		GTK_OBJECT_CLASS (eazel_installer_parent_class)->finalize (object);
	}
}

void
eazel_installer_unref (GtkObject *object)
{
	gtk_object_unref (object);
}

static void
eazel_installer_class_initialize (EazelInstallerClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*)klass;
	object_class->finalize = (void(*)(GtkObject*))eazel_installer_finalize;

	eazel_installer_parent_class = gtk_type_class (gtk_object_get_type ());
}

static void
eazel_installer_set_default_texts (EazelInstaller *installer)
{
	g_message ("Choosing default texts");
	text_labels [ERROR_UNTESTED_RPM_BASED_SYSTEM_TITLE] = g_strdup (D_ERROR_UNTESTED_RPM_BASED_SYSTEM_TITLE);
	text_labels [ERROR_UNTESTED_RPM_BASED_SYSTEM_TEXT] = g_strdup (D_ERROR_UNTESTED_RPM_BASED_SYSTEM_TEXT);
	text_labels [ERROR_NON_RPM_BASED_SYSTEM] = g_strdup (D_ERROR_NON_RPM_BASED_SYSTEM);
	text_labels [ERROR_RPM_4_NOT_SUPPORTED] = g_strdup (D_ERROR_RPM_4_NOT_SUPPORTED);
	text_labels [ERROR_REDHAT_6_ONLY] = g_strdup (D_ERROR_REDHAT_6_ONLY);
	text_labels [ERROR_REDHAT_7_ONLY] = g_strdup (D_ERROR_REDHAT_7_ONLY);
	text_labels [WAIT_LABEL] = g_strdup (D_WAIT_LABEL);
	text_labels [WAIT_LABEL_2] = g_strdup (D_WAIT_LABEL_2);
	text_labels [ERROR_LABEL] = g_strdup (D_ERROR_LABEL);
	text_labels [ERROR_LABEL_2] = g_strdup (D_ERROR_LABEL_2);
	text_labels [SPLASH_TITLE] = g_strdup (D_SPLASH_TITLE);
	text_labels [ERROR_TITLE] = g_strdup (D_ERROR_TITLE);
	text_labels [FINISHED_TITLE] = g_strdup (D_FINISHED_TITLE);
	text_labels [WHAT_TO_INSTALL_LABEL] = g_strdup (D_WHAT_TO_INSTALL_LABEL);
	text_labels [WHAT_TO_INSTALL_LABEL_SINGLE] = g_strdup (D_WHAT_TO_INSTALL_LABEL_SINGLE);
}

static gboolean
eazel_installer_setup_texts (EazelInstaller *installer, 
			     const char *dest_dir)
{
	char *url;
	char *destination;
	char *lang;
	char *ptr;
	gboolean result = TRUE;

	lang = getenv ("LANG");
	if (lang && (ptr = strchr (lang, '_')) != NULL) {
		*ptr = 0;
	}
	if (lang) {
		url = g_strdup_printf ("http://%s:%d/%s-%s.xml", 
				       installer_server ? installer_server : HOSTNAME,
				       installer_server_port ? installer_server_port : PORT_NUMBER,
				       TEXT_LIST,
				       lang);
	} else {
		url = g_strdup_printf ("http://%s:%d/%s-%s.xml", 
				       installer_server ? installer_server : HOSTNAME,
				       installer_server_port ? installer_server_port : PORT_NUMBER,
				       TEXT_LIST,
				       lang);
	}

	destination = g_strdup_printf ("%s/%s", dest_dir, TEXT_LIST);

	g_message ("Trying to contact Eazel services, ignore any 404 warnings at the next line"); 

	if (! trilobite_fetch_uri_to_file (url, destination)) {
		/* try again with proxy config */
		unlink (destination);
		if (! attempt_http_proxy_autoconfigure (installer_homedir) ||
		    ! trilobite_fetch_uri_to_file (url, destination)) {
			eazel_installer_set_default_texts (installer);
			result = FALSE;
		}
	}

	if (result) {
		/* Now I need to parse the texts and set them */
		/* FIXME bugzilla.eazel.com 1094
		 */
		eazel_installer_set_default_texts (installer);
	}

	g_free (destination);
	g_free (url);

	return result;
}

static gboolean
eazel_install_get_depends (EazelInstaller *installer, const char *dest_dir)
{
	char *url;
	char *destination;
	gboolean result = TRUE;

	url = g_strdup_printf ("http://%s:%d/%s", 
			       eazel_install_get_server (installer->service),
			       eazel_install_get_server_port (installer->service),
			       PACKAGE_LIST);

	destination = g_strdup_printf ("%s/%s", dest_dir, PACKAGE_LIST);

	g_message ("Trying to contact Eazel services...");

	if (! trilobite_fetch_uri_to_file (url, destination)) {
		/* try again with proxy config */
		unlink (destination);
		if (! attempt_http_proxy_autoconfigure (installer_homedir) ||
		    ! trilobite_fetch_uri_to_file (url, destination)) {
			jump_to_error_page (installer, NULL, ERROR_NEED_TO_SET_PROXY, "");
			rmdir (installer->tmpdir);
			result = FALSE;
		}
	}

	g_free (destination);
	g_free (url);
	return result;
}

static void
early_log_catcher (const char *domain, GLogLevelFlags flags, const char *message)
{
	if (! installer_debug) {
		return;
	}

	if (flags & G_LOG_LEVEL_DEBUG) {
		fprintf (stderr, "debug: %s\n", message);
	} else if (flags & G_LOG_LEVEL_MESSAGE) {
		fprintf (stderr, "%s\n", message);
	} else if (flags & G_LOG_LEVEL_WARNING) {
		fprintf (stderr, "warning: %s\n", message);
	} else if (flags & G_LOG_LEVEL_ERROR) {
		fprintf (stderr, "ERROR: %s\n", message);
	} else {
		/* ignore */
	}
	fflush (stderr);
}

/* call this almost immediately so that most log messages from libraries are caught */
static void
catch_early_logs (void)
{
	g_log_set_handler (G_LOG_DOMAIN,
			   G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_WARNING | G_LOG_LEVEL_ERROR, 
			   (GLogFunc)early_log_catcher,
			   NULL);
}

static void
start_logging (EazelInstaller *installer)
{
	int flags;
	int fd;
	FILE *fp;
	struct stat statbuf, lstatbuf;
	char *filename;

	eazel_install_log_to_stderr (installer->service, installer_spam ? TRUE : FALSE);

	/* try opening our favorite logfile */
	flags = O_WRONLY | O_CREAT | O_APPEND;
#ifdef O_NOFOLLOW
	/* wow, linux defines this but it's completely non-functional on linux. :( */
	flags |= O_NOFOLLOW;
#endif
	filename = g_strdup_printf ("%s/%s", installer_tmpdir, LOGFILE);
	fd = open (filename, flags, 0600);
	/* make sure that:
	 *  - owned by root (uid = 0)
	 *  - the mode is X00 (group/other can't read/write/execute)
	 *  - it's a regular file
	 *  - we didn't follow a symlink
	 *  - hardlink count = 1
	 */
	if ((fd >= 0) && (fstat (fd, &statbuf) == 0) &&
	    (lstat (filename, &lstatbuf) == 0) &&
	    ((lstatbuf.st_mode & S_IFLNK) != S_IFLNK) &&
	    ((statbuf.st_mode & 0077) == 0) &&
	    (statbuf.st_mode & S_IFREG) &&
	    (statbuf.st_nlink == 1) &&
	    (statbuf.st_uid == 0)) {
		/* this is our file -- truncate and start over */
		fprintf (stderr, "Writing logfile to %s ...\n", filename);
		ftruncate (fd, 0);
		fp = fdopen (fd, "wt");
		eazel_install_set_log (installer->service, fp);
	} else {
		if (fd >= 0) {
			close (fd);
		}
		fprintf (stderr, "Can't write to %s :(\n", filename);
	}
	g_free (filename);

	g_message ("Eazel-Installer v" VERSION " (build " BUILD_DATE ")");
}


static void
get_candidate_dirs (EazelInstall *install, char *dir)
{
	DIR *dirfd;
	struct dirent *file;
	char *candidate;
	struct stat statbuf;

	dirfd = opendir (dir);
	if (dirfd == NULL) {
		return;
	}
	while ((file = readdir (dirfd)) != NULL) {
		candidate = g_strdup_printf ("%s/%s", dir, file->d_name);
		if ((lstat (candidate, &statbuf) == 0) &&
		    (statbuf.st_mode & S_IFDIR) &&
		    ((statbuf.st_mode & S_IFLNK) != S_IFLNK) &&
		    (statbuf.st_nlink == 2)) {
			if ((strstr (file->d_name, "RPM") != NULL) ||
			    (strstr (file->d_name, "package") != NULL)) {
				/* good candidate! */
				printf ("candidate: '%s'\n", candidate);
				eazel_install_add_repository (install, candidate);
			}
		}
		g_free (candidate);
	}
	closedir (dirfd);
}

/* look for a mounted cdrom:
 * anything with "cdrom" or "iso9660" in the name
 */
static void
search_for_local_cds (EazelInstall *install)
{
	FILE *fp;
	char line[256];
	char *p, *q;
	char *dir;

	fp = fopen ("/proc/mounts", "r");
	if (fp == NULL) {
		g_warning ("Couldn't open /proc/mounts");
		return;
	}
	while (! feof (fp)) {
		fgets (line, 250, fp);
		if (feof (fp)) {
			break;
		}
		line[250] = '\0';
		if ((strstr (line, "cdrom") != NULL) ||
		    (strstr (line, "iso9660") != NULL)) {
			/* candidate: 2nd field is the mountpoint */
			p = strchr (line, ' ');
			if (p != NULL) {
				p++;
				q = strchr (p, ' ');
				if (q != NULL) {
					dir = g_strndup (p, q-p);
					get_candidate_dirs (install, dir);
					g_free (dir);
				}
			}
		}
	}
	fclose (fp);

	if (installer_cache_dir != NULL) {
		eazel_install_add_repository (install, installer_cache_dir);
	}
}

/* if there's an older tmpdir left over from a previous attempt, use it */
#define TMPDIR_PREFIX "eazel-installer."
static char *
find_old_tmpdir (void)
{
	DIR *dirfd;
	struct dirent *file;
	char *old_tmpdir = NULL;
	char *old_package_list;
	struct stat statbuf;

	dirfd = opendir (installer_tmpdir);
	if (dirfd == NULL) {
		return NULL;
	}
	while ((file = readdir (dirfd)) != NULL) {
		if ((old_tmpdir == NULL) && (strlen (file->d_name) > strlen (TMPDIR_PREFIX)) &&
		    (strncmp (file->d_name, TMPDIR_PREFIX, strlen (TMPDIR_PREFIX)) == 0)) {
			old_tmpdir = g_strdup_printf ("%s/%s", installer_tmpdir, file->d_name);
			if ((lstat (old_tmpdir, &statbuf) == 0) &&
			    ((statbuf.st_mode & 0077) == 0) &&
			    (statbuf.st_mode & S_IFDIR) &&
			    ((statbuf.st_mode & S_IFLNK) != S_IFLNK) &&
			    (statbuf.st_nlink == 2) &&
			    (statbuf.st_uid == 0)) {
				/* acceptable */
				log_debug ("found an old tmpdir: %s", old_tmpdir);
				/* make sure old package list isn't hanging around */
				old_package_list = g_strdup_printf ("%s/%s", old_tmpdir, PACKAGE_LIST);
				unlink (old_package_list);
				g_free (old_package_list);
				chmod (old_tmpdir, 0700);
			} else {
				g_free (old_tmpdir);
				old_tmpdir = NULL;
			}
		}
	}
	closedir (dirfd);

	return old_tmpdir;
}

static void
add_singular_package (EazelInstaller *installer,
		      char *package_name)
{
	CategoryData *cat = categorydata_new ();
	PackageData *pack = packagedata_new ();
	struct utsname ub;
	
	uname (&ub);
	g_warning ("Ugh, no categories");
	cat->name = g_strdup (package_name);
	pack->name = g_strdup (package_name);
	pack->archtype = g_strdup (ub.machine);
	cat->packages = g_list_prepend (NULL, pack);
	installer->categories = g_list_prepend (NULL, cat);
}

static void
eazel_installer_initialize (EazelInstaller *object)
{
	EazelInstaller *installer;
	GList *iterator;
	char *tmpdir;
	char *package_destination;
	char *splash_text = NULL;
	char *finish_text = NULL;
	int tries;
	GtkWidget *vbox;

	g_assert (object != NULL);
	g_assert (IS_EAZEL_INSTALLER (object));

	catch_early_logs ();

	installer = EAZEL_INSTALLER (object);

	/* we have to start SOMEWHERE.  several errors could occur between now and when we finally get the
	 * error texts from the server -- so let's just use some common-sense defaults till then.
	 */
	eazel_installer_set_default_texts (installer);

	tmpdir = find_old_tmpdir ();
	if (tmpdir == NULL) {
		/* attempt to create a directory we can use */
#define RANDCHAR ('A' + (rand () % 23))
		srand (time (NULL));
		for (tries = 0; tries < 50; tries++) {
			tmpdir = g_strdup_printf ("%s/eazel-installer.%c%c%c%c%c%c%d",
						  installer_tmpdir,
						  RANDCHAR, RANDCHAR, RANDCHAR, RANDCHAR,
						  RANDCHAR, RANDCHAR, (rand () % 1000));
			if (mkdir (tmpdir, 0700) == 0) {
				break;
			}
			g_free (tmpdir);
		}
		if (tries == 50) {
			g_error (_("Cannot create temporary directory"));
		}
	}

	installer->tmpdir = tmpdir;

	installer->test = installer_test;
	installer->debug = installer_debug;

	installer->must_have_categories = NULL;
	installer->implicit_must_have = NULL;
	installer->dont_show = NULL;
	installer->failure_info = NULL;
	installer->install_categories = NULL;
	installer->force_remove_categories = NULL;
	installer->successful = TRUE;
	installer->uninstalling = FALSE;
	installer->packages_possible_broken = NULL;
	package_destination = g_strdup_printf ("%s/%s", installer->tmpdir, PACKAGE_LIST);
	installer->downloaded_anything = FALSE;

	eazel_installer_setup_texts (installer, tmpdir);
	installer->window = create_window (installer);
	create_what_to_do_page (installer);
	create_install_page (installer);

	gtk_widget_show (installer->window);
	if (! check_system (installer)) {
		return;
	}

	installer->service =  
		EAZEL_INSTALL (gtk_object_new (TYPE_EAZEL_INSTALL,
					       "verbose", TRUE,
					       "silent", FALSE,
					       "debug", installer->debug ? TRUE : FALSE,
					       "test", installer->test ? TRUE : FALSE, 
					       "force", installer_force ? TRUE : FALSE,
					       "depend", FALSE,
					       "update", TRUE,
					       "uninstall", FALSE,
					       "downgrade", TRUE,
					       "protocol", installer_local ? PROTOCOL_LOCAL: PROTOCOL_HTTP,
					       "tmp_dir", tmpdir,
					       "rpmrc_file", RPMRC,
					       "server", installer_server ? installer_server : HOSTNAME,
					       "server_port", 
					       installer_server_port ? installer_server_port : PORT_NUMBER,
					       "package_list", package_destination, 
					       "package_list_storage_path", PACKAGE_LIST,
					       "transaction_dir", installer_tmpdir,
					       "cgi_path", installer_cgi_path ? installer_cgi_path : CGI_PATH,
					       NULL));

	search_for_local_cds (installer->service);

	gnome_druid_set_buttons_sensitive (installer->druid, FALSE, FALSE, TRUE);

	/* show what we have so far */
	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}

	if (installer_package == NULL) {
		/* used to do RPM4 and eazel-hacking checks here... no point anymore (both should work) */
	}

	installer->problem = eazel_install_problem_new ();
	
	gtk_signal_connect (GTK_OBJECT (installer->service),
			    "file_conflict_check",
			    GTK_SIGNAL_FUNC (conflict_check),
			    installer);
	gtk_signal_connect (GTK_OBJECT (installer->service), 
			    "download_progress", 
			    GTK_SIGNAL_FUNC (eazel_download_progress),
			    installer);
	gtk_signal_connect (GTK_OBJECT (installer->service), 
			    "install_progress", 
			    GTK_SIGNAL_FUNC (eazel_install_progress), 
			    installer);
	gtk_signal_connect (GTK_OBJECT (installer->service), 
			    "preflight_check", 
			    GTK_SIGNAL_FUNC (eazel_install_preflight), 
			    installer);
	gtk_signal_connect (GTK_OBJECT (installer->service), 
			    "dependency_check", 
			    GTK_SIGNAL_FUNC (eazel_install_dep_check), 
			    installer);
	gtk_signal_connect (GTK_OBJECT (installer->service), 
			    "download_failed", 
			    download_failed, 
			    installer);
	gtk_signal_connect (GTK_OBJECT (installer->service), 
			    "install_failed", 
			    GTK_SIGNAL_FUNC (install_failed), 
			    installer);
	gtk_signal_connect (GTK_OBJECT (installer->service), 
			    "uninstall_failed", 
			    GTK_SIGNAL_FUNC (uninstall_failed), 
			    installer);
	gtk_signal_connect (GTK_OBJECT (installer->service), 
			    "done", 
			    GTK_SIGNAL_FUNC (install_done), 
			    installer);

	start_logging (installer);

	/* now this also fetches the category deps too */
	if (! eazel_install_get_depends (installer, tmpdir)) {
		/* already posted error */
		return;
	}

	if (installer_package==NULL) {
		installer->categories = parse_local_xml_package_list (package_destination, 
								      &splash_text, 
								      &finish_text);
	} else {
		add_singular_package (installer, installer_package);
	}

	if (!installer->categories) {
		add_singular_package (installer, "nautilus");
	}

	vbox = GTK_WIDGET (gtk_object_get_data (GTK_OBJECT (installer->window), "vbox3"));
	if (installer->categories && installer->categories->next) {
		/* more than one category */
		for (iterator = installer->categories; iterator; iterator=iterator->next) {
			eazel_installer_add_category (installer, (CategoryData*)iterator->data, FALSE);
			gtk_box_add_padding (vbox, 0, 5);
		}
	} else {
		/* single category */
		eazel_installer_add_category (installer, (CategoryData *)installer->categories->data, TRUE);
	}

	g_free (package_destination);

	/* redraw start page, now that we have splash text */
	draw_splash_text (installer, splash_text);
	g_free (splash_text);

	/* make good-finish page, now that we have the finish text for it */
	installer->finish_good = GNOME_DRUID_PAGE (create_finish_page_good (GTK_WIDGET (installer->druid),
									    installer->window,
									    finish_text));
	g_free (finish_text);

	gtk_signal_connect (GTK_OBJECT (installer->finish_good), "prepare",
			    GTK_SIGNAL_FUNC (prep_finish),
			    installer);
	gtk_signal_connect (GTK_OBJECT (installer->finish_good), "finish",
			    GTK_SIGNAL_FUNC (druid_finish),
			    installer);

#ifdef THAT_DAMN_CHECKBOX
	gnome_druid_set_buttons_sensitive (installer->druid, FALSE, FALSE, TRUE);
#endif

	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}
}

GtkType
eazel_installer_get_type() {
	static GtkType installer_type = 0;

	if (!installer_type)
	{
		static const GtkTypeInfo installer_info =
		{
			"EazelInstaller",
			sizeof (EazelInstaller),
			sizeof (EazelInstallerClass),
			(GtkClassInitFunc) eazel_installer_class_initialize,
			(GtkObjectInitFunc) eazel_installer_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		/* Get a unique GtkType */
		installer_type = gtk_type_unique (gtk_object_get_type (), &installer_info);
	}

	return installer_type;
}

/*
  The _new method simply builds the service
  using gtk_object_new
*/
EazelInstaller *
eazel_installer_new (void)
{
	EazelInstaller *installer;

	installer = EAZEL_INSTALLER (gtk_object_new (TYPE_EAZEL_INSTALLER, NULL));
	
	return installer;
}
