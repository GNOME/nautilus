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
#include <libtrilobite/helixcode-utils.h>
#include <eazel-install-xml-package-list.h>
#include <eazel-install-protocols.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/utsname.h>

#include <nautilus-druid.h>
#include <nautilus-druid-page-eazel.h>

#include "installer.h"
#include "callbacks.h"
#include "installer.h"
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

#define HOSTNAME "services.eazel.com"
#define PORT_NUMBER 8888
#define CGI_PATH "/catalog/find"
#define TMP_DIR "/tmp/eazel-install"
#define RPMRC "/usr/lib/rpm/rpmrc"
#define REMOTE_RPM_DIR "/RPMS"
#define PACKAGE_LIST	"package-list.xml"

#define FONT_NORM_BOLD	_("-adobe-helvetica-bold-r-normal-*-*-120-*-*-p-*-*-*,*-r-*")
#define FONT_NORM	_("-adobe-helvetica-medium-r-normal-*-*-120-*-*-p-*-*-*,*-r-*")
#define FONT_TITLE	_("-adobe-helvetica-medium-r-normal-*-24-*-*-*-p-*-*-*,*-r-*")

#define CONTENT_X	64
#define CONTENT_Y	63

#define ERROR_SYMBOL_X	67
#define ERROR_SYMBOL_Y  59

#define ASSUMED_MAX_DOWNLOAD	(75*1024*1024)		/* 75MB assumed to be the max downloaded */
			 	/* yes, virginia, people actually broke the 50MB limit! */

#define ERROR_NEED_TO_SET_PROXY _("I can't reach the Eazel servers.  This could be because the\n" \
				  "Eazel servers are down, or more likely, because you need to\n" \
				  "use a web proxy to access external web servers, and I couldn't\n" \
				  "figure out your proxy configuration.\n\n" \
				  "If you know you have a web proxy, you can try again by setting\n" \
				  "the environment variable \"http_proxy\" to the URL of your proxy\n" \
 				  "server, and then restarting Eazel Installer.")
#define WAIT_LABEL	_("Please wait while we download and install the files selected.")
#define WAIT_LABEL_2	_("Now starting the install process.  This will take some time, so\n" \
			  "please be patient.")
#define ERROR_LABEL	_("The installer was not able to complete the installation of the\n" \
			  "selected files.  Here's why:")
#define ERROR_LABEL_2	_("Look for possible solutions to this problem at:\n" \
 			  "        http://nautilus.eazel.com/faq.html\n" \
			  "Once you have resolved the problem, please restart the installer.")
#define ERROR_TITLE	_("An error has occurred")
#define RETRY_LABEL	_("An installation problem has been encountered, but we think we can\n" \
			  "fix it.  We would like to try the following actions, but since there\n" \
			  "are items involved that might be important to you, we thought we'd\n" \
			  "check first.")
#define RETRY_TITLE	_("Just so you know...")
#define FINISHED_TITLE	_("Congratulations!")

#define NAUTILUS_INSTALLER_RELEASE

int installer_debug = 0;
int installer_test = 0;
int installer_force = 0;
int installer_local = 0;
int installer_no_helix = 0;
char *installer_server =NULL;
int installer_server_port = 0;
char *installer_cgi_path = NULL;

static void check_if_next_okay (GnomeDruidPage *page, void *unused, EazelInstaller *installer);


static GtkObjectClass *eazel_installer_parent_class;

static void
get_pixmap_x_y (char **xpmdata, int *x, int *y)
{
	char *ptr;

	ptr = strchr (xpmdata[0], ' ');
	if (ptr == NULL) {
		*x = *y = 0;
		return;
	}
	*x = atoi (xpmdata[0]);
	*y = atoi (ptr);
}

static GdkPixbuf*
create_pixmap (GtkWidget *widget,
	       char **xpmdata)
{
	GdkColormap *colormap;                                                        
	GdkPixmap *gdkpixmap;                                                         
	GdkBitmap *mask;	
	GdkPixbuf *pixbuf;
	int x, y;

	get_pixmap_x_y (xpmdata, &x, &y);

	colormap = gtk_widget_get_colormap (widget);
	
	gdkpixmap = gdk_pixmap_colormap_create_from_xpm_d (NULL, 
							   colormap, 
							   &mask,
							   NULL, 
							   (gchar**)xpmdata); 
	
	g_assert (gdkpixmap != NULL);

	pixbuf = gdk_pixbuf_get_from_drawable (NULL,
					       gdkpixmap,
					       colormap,
					       0,0,0,0, 
					       x, y);

	gdk_pixmap_unref (gdkpixmap);   
	if (mask != NULL) {
		gdk_bitmap_unref (mask);
	}

	return pixbuf;     
}

static GtkWidget *
gtk_label_new_with_font (const char *text, const char *fontname)
{
	GtkWidget *label;
	GtkStyle *style;

	label = gtk_label_new (text);
	style = gtk_style_copy (label->style);
	gdk_font_unref (style->font);
	/* oh how low we've sunk... */
	style->font = gdk_fontset_load (fontname);
	gtk_widget_set_style (label, style);
	gtk_style_unref (style);

	return label;
}

static void
add_padding_to_box (GtkWidget *box, int pad_x, int pad_y)
{
	GtkWidget *filler;

	filler = gtk_label_new ("");
	gtk_widget_set_usize (filler, pad_x ? pad_x : 1, pad_y ? pad_y : 1);
	gtk_widget_show (filler);
	gtk_box_pack_start (GTK_BOX (box), filler, FALSE, FALSE, 0);
}

GtkWidget*
create_what_to_do_page (GtkWidget *druid, GtkWidget *window)
{
	GtkWidget *what_to_do_page;
	GtkWidget *vbox;
	GtkWidget *title;
	GtkWidget *hbox;

	what_to_do_page = nautilus_druid_page_eazel_new_with_vals (NAUTILUS_DRUID_PAGE_EAZEL_OTHER, 
								   "", "", NULL, NULL,
								   create_pixmap (GTK_WIDGET (window), bootstrap_background));

	gtk_widget_set_name (what_to_do_page, "what_to_do_page");
	gtk_widget_ref (what_to_do_page);
	gtk_object_set_data_full (GTK_OBJECT (window), "what_to_do_page", what_to_do_page,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show_all (what_to_do_page);
	gnome_druid_append_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (what_to_do_page));

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_object_set_data_full (GTK_OBJECT (window), "vbox3", vbox, (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (vbox);
	gtk_widget_set_uposition (vbox, CONTENT_X, CONTENT_Y);

	title = gtk_label_new_with_font (_("What would you like to install?"), FONT_TITLE);
	gtk_label_set_justify (GTK_LABEL (title), GTK_JUSTIFY_LEFT);
	gtk_widget_show (title);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), title, FALSE, FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	nautilus_druid_page_eazel_put_widget (NAUTILUS_DRUID_PAGE_EAZEL (what_to_do_page), vbox);

	return what_to_do_page;
}

GtkWidget*
create_install_page (GtkWidget *druid, GtkWidget *window)
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
								create_pixmap (GTK_WIDGET (window), bootstrap_background));
	gtk_widget_set_name (install_page, "install_page");
	gtk_widget_ref (install_page);
	gtk_object_set_data_full (GTK_OBJECT (window), "install_page", install_page,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show_all (install_page);
	gnome_druid_append_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (install_page));

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_set_name (vbox, "install/vbox");
	gtk_widget_set_uposition (vbox, CONTENT_X, CONTENT_Y);
	gtk_widget_ref (vbox);
	gtk_object_set_data_full (GTK_OBJECT (window), "vbox", vbox,
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

	wait_label = gtk_label_new (WAIT_LABEL);
	gtk_widget_set_name (wait_label, "label_top");
	gtk_label_set_justify (GTK_LABEL (wait_label), GTK_JUSTIFY_LEFT);
	gtk_widget_ref (wait_label);
	gtk_object_set_data_full (GTK_OBJECT (window), "label_top", wait_label,
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
	gtk_object_set_data_full (GTK_OBJECT (window), "header_single", download_label,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (download_label);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), download_label, FALSE, FALSE, 40);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	progressbar1 = gtk_progress_bar_new ();
	gtk_widget_set_name (progressbar1, "progressbar_single");
	gtk_widget_ref (progressbar1);
	gtk_object_set_data_full (GTK_OBJECT (window), "progressbar_single", progressbar1,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (progressbar1);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), progressbar1, FALSE, FALSE, 50);
	gtk_widget_set_usize (progressbar1, 300, 20);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox,  FALSE, FALSE, 3);

	label_single = gtk_label_new (_("Eskil's been licking mushrooms again"));
	gtk_widget_set_name (label_single, "label_single");
	gtk_label_set_justify (GTK_LABEL (label_single), GTK_JUSTIFY_LEFT);
	gtk_widget_ref (label_single);
	gtk_object_set_data_full (GTK_OBJECT (window), "label_single", label_single,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label_single);

	label_single_2 = gtk_label_new ("");
	gtk_widget_set_name (label_single_2, "label_single_2");
	gtk_label_set_justify (GTK_LABEL (label_single_2), GTK_JUSTIFY_LEFT);
	gtk_widget_ref (label_single_2);
	gtk_object_set_data_full (GTK_OBJECT (window), "label_single_2", label_single_2,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label_single_2);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_set_name (hbox, "hbox_label_single");
	gtk_widget_ref (hbox);
	gtk_object_set_data_full (GTK_OBJECT (window), "hbox_label_single", label_single,
				  (GtkDestroyNotify) gtk_widget_unref);
	add_padding_to_box (hbox, 50, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label_single, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label_single_2, FALSE, FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox,  FALSE, FALSE, 0);

	add_padding_to_box (vbox, 0, 15);

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
	gtk_object_set_data_full (GTK_OBJECT (window), "progressbar_overall", progressbar2,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (progressbar2);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), progressbar2, FALSE, FALSE, 50);
	gtk_widget_set_usize (progressbar2, 300, 20);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox,  FALSE, FALSE, 3);

	label_overall = gtk_label_new (_("Downloading packages required to install Nautilus"));
	gtk_widget_set_name (label_overall, "label_overall");
	gtk_label_set_justify (GTK_LABEL (label_overall), GTK_JUSTIFY_LEFT);
	gtk_widget_ref (label_overall);
	gtk_object_set_data_full (GTK_OBJECT (window), "label_overall", label_overall,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label_overall);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label_overall, FALSE, FALSE, 50);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox,  FALSE, FALSE, 0);

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
	char *temp;

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

	title = gtk_label_new_with_font (ERROR_TITLE, FONT_TITLE);
	gtk_label_set_justify (GTK_LABEL (title), GTK_JUSTIFY_LEFT);
	gtk_widget_show (title);
	pixmap = create_pixmap_widget (error_page, error_symbol);
	gtk_widget_show (pixmap);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), pixmap, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), title, FALSE, FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	add_padding_to_box (vbox, 0, 20);

	label = gtk_label_new (text);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (label), FALSE);
	gtk_widget_show (label);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 30);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	add_padding_to_box (vbox, 0, 15);

	for (iter = g_list_first (bullets); iter != NULL; iter = g_list_next (iter)) {
		temp = g_strdup_printf ("\xB7 %s", (char *)(iter->data));
		label = gtk_label_new_with_font (temp, FONT_NORM_BOLD);
		g_free (temp);
		gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
		gtk_widget_show (label);
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 45);
		gtk_widget_show (hbox);
		gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	}

	add_padding_to_box (vbox, 0, 15);

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

static gboolean
start_over (GnomeDruidPage *druid_page, GnomeDruid *druid, EazelInstaller *installer)
{
	GtkWidget *install_page;
	CategoryData *category;
	PackageData *package;
	char *arch;
	DistributionInfo distro;
	GList *iter, *iter2;

	LOG_DEBUG (("JES: start over\n"));

	/* figure out current arch */
	arch = NULL;
	distro.name = DISTRO_UNKNOWN;
	for (iter = g_list_first (installer->categories); iter != NULL; iter = g_list_next (iter)) {
		category = (CategoryData *)(iter->data);
		for (iter2 = g_list_first (category->packages); iter2 != NULL; iter2 = g_list_next (iter2)) {
			package = (PackageData *)(iter2->data);
			if (package && package->archtype) {
				arch = package->archtype;
			}
			if (package && (package->distribution.name != DISTRO_UNKNOWN)) {
				distro = package->distribution;
			}
			if ((arch != NULL) && (distro.name != DISTRO_UNKNOWN)) {
				/* done. */
				iter2 = iter = NULL;
			}
		}
	}

	category = categorydata_new ();
	category->name = g_strdup ("Fake extra category");

	while (installer->additional_packages) {
		package = packagedata_new ();
		package->name = installer->additional_packages->data;	/* it's already a copy */
		package->archtype = g_strdup (arch);
		package->distribution = distro;
		package->toplevel = TRUE;
		category->packages = g_list_prepend (category->packages, package);
		installer->additional_packages = g_list_remove (installer->additional_packages,
								installer->additional_packages->data);
	}

	installer->categories = g_list_prepend (installer->categories, category);

	/* clear out failure info */
	g_list_foreach (installer->failure_info, (GFunc) g_free, NULL);
	g_list_free (installer->failure_info);
	installer->failure_info = NULL;

#ifdef ROBEYS_PARANOID_DEBUG_OUTPUT
	printf ("##########  NEW PACKAGE LIST  ##########\n");
	for (iter = g_list_first (installer->categories); iter != NULL; iter = g_list_next (iter)) {
		category = (CategoryData *)(iter->data);
		printf ("<<< CATEGORY: %s >>>\n", category->name);
		dump_packages (category->packages);
	}
	printf ("##########  END OF LIST  ##########\n");
#endif

	install_page = gtk_object_get_data (GTK_OBJECT (installer->window), "install_page");
	gnome_druid_set_page (installer->druid, GNOME_DRUID_PAGE (install_page));
	return TRUE;	/* yes, i handled the page change */
}

static gboolean
dont_start_over (GnomeDruidPage *druid_page, GnomeDruid *druid, EazelInstaller *installer)
{
	LOG_DEBUG (("NE: give up\n"));
	jump_to_error_page (installer, installer->failure_info, ERROR_LABEL, ERROR_LABEL_2);
	return TRUE;	/* go to error page instead of cancelling */
}

/* give the user an opportunity to retry the install, with new info */
static void
jump_to_retry_page (EazelInstaller *installer)
{
	GtkWidget *retry_page;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *pixmap;
	GtkWidget *title;
	GtkWidget *label;
	GList *iter;
	char *temp;

	retry_page = nautilus_druid_page_eazel_new_with_vals (NAUTILUS_DRUID_PAGE_EAZEL_OTHER,
							      "", "", NULL, NULL,
							      create_pixmap (GTK_WIDGET (installer->window),
									     bootstrap_background));
	gtk_widget_show (retry_page);
	gnome_druid_append_page (GNOME_DRUID (installer->druid), GNOME_DRUID_PAGE (retry_page));

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_set_uposition (vbox, ERROR_SYMBOL_X, ERROR_SYMBOL_Y);
	gtk_widget_show (vbox);
	nautilus_druid_page_eazel_put_widget (NAUTILUS_DRUID_PAGE_EAZEL (retry_page), vbox);

	title = gtk_label_new_with_font (RETRY_TITLE, FONT_TITLE);
	gtk_label_set_justify (GTK_LABEL (title), GTK_JUSTIFY_LEFT);
	gtk_widget_show (title);
	pixmap = create_pixmap_widget (retry_page, error_symbol);
	gtk_widget_show (pixmap);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), pixmap, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), title, FALSE, FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	add_padding_to_box (vbox, 0, 20);

	label = gtk_label_new (RETRY_LABEL);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (label), FALSE);
	gtk_widget_show (label);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 30);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	add_padding_to_box (vbox, 0, 15);

	for (iter = g_list_first (installer->additional_packages); iter != NULL; iter = g_list_next (iter)) {
		temp = g_strdup_printf ("\xB7 %s", (char *)(iter->data));
		label = gtk_label_new_with_font (temp, FONT_NORM_BOLD);
		g_free (temp);
		gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
		gtk_widget_show (label);
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 45);
		gtk_widget_show (hbox);
		gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	}

	add_padding_to_box (vbox, 0, 15);

#if 0
	label = gtk_label_new (RETRY_LABEL_2);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (label), FALSE);
	gtk_widget_show (label);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 30);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	add_padding_to_box (vbox, 0, 15);

	hbox = gtk_hbox_new (FALSE, 0);
	button = gtk_button_new_with_label ("Jes!");
	gtk_widget_show (button);
	gtk_signal_connect (GTK_OBJECT (button), "clicked", GTK_SIGNAL_FUNC (start_over), installer);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 5);
	button = gtk_button_new_with_label ("Ne!");
	gtk_widget_show (button);
	gtk_signal_connect (GTK_OBJECT (button), "clicked", GTK_SIGNAL_FUNC (dont_start_over), installer);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 5);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
#endif

	gtk_signal_connect (GTK_OBJECT (retry_page), "prepare",
			    GTK_SIGNAL_FUNC (prep_retry),
			    installer);
	gtk_signal_connect (GTK_OBJECT (retry_page), "next",
			    GTK_SIGNAL_FUNC (start_over),
			    installer);
	gtk_signal_connect (GTK_OBJECT (retry_page), "cancel",
			    GTK_SIGNAL_FUNC (dont_start_over),
			    installer);
	gnome_druid_set_page (installer->druid, GNOME_DRUID_PAGE (retry_page));
}

GtkWidget*
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

	title = gtk_label_new_with_font (FINISHED_TITLE, FONT_TITLE);
	gtk_label_set_justify (GTK_LABEL (title), GTK_JUSTIFY_LEFT);
	gtk_widget_show (title);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), title, FALSE, FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	add_padding_to_box (vbox, 0, 20);

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
	GtkWidget *install_page;
	GtkWidget *druid;
	GtkWidget *start_page;
	GtkWidget *what_to_do_page;
	int x, y;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);	
	gtk_widget_set_name (window, "window");
	gtk_object_set_data (GTK_OBJECT (window), "window", window);
	gtk_window_set_title (GTK_WINDOW (window), _("Eazel Installer"));
	gtk_window_set_policy (GTK_WINDOW (window), FALSE, FALSE, TRUE);
	get_pixmap_x_y (bootstrap_background, &x, &y);
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

	what_to_do_page = create_what_to_do_page (druid, window);
	install_page = create_install_page (druid, window);

	gtk_signal_connect (GTK_OBJECT (druid), "cancel",
			    GTK_SIGNAL_FUNC (druid_cancel),
			    installer);
	gtk_signal_connect (GTK_OBJECT (druid), "destroy",
			    GTK_SIGNAL_FUNC (druid_delete),
			    installer);
	gtk_signal_connect (GTK_OBJECT (install_page), "finish",
			    GTK_SIGNAL_FUNC (druid_finish),
			    installer);
	gtk_signal_connect (GTK_OBJECT (install_page), "prepare",
			    GTK_SIGNAL_FUNC (prep_install),
			    installer);
	gtk_signal_connect (GTK_OBJECT (what_to_do_page), "prepare",
			    GTK_SIGNAL_FUNC (check_if_next_okay),
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
	char *temp;
	double percent;

	label_single = gtk_object_get_data (GTK_OBJECT (installer->window), "label_single");
	label_overall = gtk_object_get_data (GTK_OBJECT (installer->window), "label_overall");
	progressbar = gtk_object_get_data (GTK_OBJECT (installer->window), "progressbar_single");
	progress_overall = gtk_object_get_data (GTK_OBJECT (installer->window), "progressbar_overall");

	if (amount == 0) {
		temp = g_strdup_printf (_("Installing the %s package"), package->name);
		gtk_label_set_text (GTK_LABEL (label_single), temp);
		g_free (temp);

		gtk_progress_configure (GTK_PROGRESS (progressbar), 0, 0, (float)(total/1024));		

		LOG_DEBUG (("\n"));
	}

/*
	if (installer_debug) {
		float pct;
		pct = ( (total > 0) ? ((float) ((((float) amount) / total) * 100)): 100.0);
		LOG_DEBUG (("Install Progress - %s - %d %d (%d %d) %% %f\r", 
			    package->name?package->name:"(null)", 
			    amount, total, 
			    total_size_completed, total_size, 
			    pct));
	}
	if (amount == total) {
		LOG_DEBUG (("\n"));
	}
*/

	gtk_progress_set_value (GTK_PROGRESS (progressbar), 
				(float)(amount/1024 > total/1024 ? total/1024 : amount/1024));
	percent = (double)(total_size_completed * 50.0) / (total_size ? total_size : 0.1);
	percent += 50.0;
	gtk_progress_set_value (GTK_PROGRESS (progress_overall), percent);

	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}
}


static void 
eazel_download_progress (EazelInstall *service, 
			 const char *name,
			 int amount, 
			 int total,
			 EazelInstaller *installer) 
{
	GtkWidget *progress_single;
	GtkWidget *progress_overall;
	GtkWidget *label_single;
	GtkWidget *label_single_2;
	char *temp;
	int amount_KB = (amount+512)/1024;
	int total_KB = (total+512)/1024;

	label_single = gtk_object_get_data (GTK_OBJECT (installer->window), "label_single");
	label_single_2 = gtk_object_get_data (GTK_OBJECT (installer->window), "label_single_2");
	progress_single = gtk_object_get_data (GTK_OBJECT (installer->window), "progressbar_single");
	progress_overall = gtk_object_get_data (GTK_OBJECT (installer->window), "progressbar_overall");

	if (amount == 0) {
		gtk_progress_configure (GTK_PROGRESS (progress_single), 0, 0, (float)total);
		gtk_progress_configure (GTK_PROGRESS (progress_overall), 0, 0, (float)ASSUMED_MAX_DOWNLOAD);
		temp = g_strdup_printf ("Getting package \"%s\"  ", name);
		gtk_label_set_text (GTK_LABEL (label_single), temp);
		g_free (temp);
		installer->last_KB = 0;
	}

/*
	if (installer_debug) {
		float pct;
		pct = ( (total > 0) ? ((float) ((((float) amount) / total) * 100)): 100.0);
		LOG_DEBUG (("DOWNLOAD Progress - %s - %d %d %% %f\r", 
			    name?name:"(null)", amount, total, pct));
	}
*/

	gtk_progress_set_value (GTK_PROGRESS (progress_single), (float)amount);
	gtk_progress_set_value (GTK_PROGRESS (progress_overall), (float)(installer->total_bytes_downloaded + amount));

	if ((amount_KB >= installer->last_KB+10) || (amount_KB == total_KB) || (amount_KB == 0)) {
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

static char *
get_required_name (const PackageData *pack)
{
	char *result = NULL;
	char *temp, *temp2;

	if (pack==NULL) {
		result = NULL;
	} else if (pack->name && pack->version) {
		if (strstr (pack->version, "EazelSourceSnapshot") != NULL) {
			/* this crap is too long to display ! */
			temp = g_strdup (pack->version);
			temp2 = strstr (temp, "EazelSourceSnapshot");
			strcpy (temp2, "ESS");
			result = g_strdup_printf ("%s v%s", pack->name, temp);
			g_free (temp);
		} else {
			result = g_strdup_printf ("%s v%s", pack->name, pack->version);
		}
	} else if (pack->name) {
		result = g_strdup_printf ("%s", pack->name);
	} else if (pack->eazel_id != NULL) {
		result = g_strdup_printf ("Eazel package %s", pack->eazel_id);
	} else if (pack->provides->data != NULL) {
		result = g_strdup (pack->provides->data);
	} else {
		/* what the--?!  WHO ARE YOU! */
		result = g_strdup ("another package");
	}
	
	return result;
}

static void
get_detailed_errors_foreach (PackageData *pack, GetErrorsForEachData *data)
{
	char *message = NULL;
	char *required;
	char *required_by;
	gboolean recoverable_error = FALSE;
	EazelInstaller *installer = data->installer;
	PackageData *previous_pack = NULL;

	if (data->path) {
		previous_pack = (PackageData*)(data->path->data);
	}
	required = get_required_name (pack);
	required_by = get_required_name (previous_pack);

	LOG_DEBUG (("traversing error tree: package (%s) status (%d)\n", required, pack->status));

	switch (pack->status) {
	case PACKAGE_UNKNOWN_STATUS:
		/* assume unknown status is caused by another package's breakage */
		recoverable_error = TRUE;
		break;
	case PACKAGE_SOURCE_NOT_SUPPORTED:
		break;
	case PACKAGE_FILE_CONFLICT:
		message = g_strdup_printf (_("%s had a file conflict with %s"), required, required_by);
		if ((pack->name != NULL) && (strcmp (pack->name, previous_pack->name) != 0)) {
			recoverable_error = TRUE;
			installer->additional_packages = g_list_prepend (installer->additional_packages,
									 g_strdup (pack->name));
		}
		g_warning ("%s file nuked %s", 
			   required_by,
			   required);		
		break;
	case PACKAGE_DEPENDENCY_FAIL:
		if (pack->soft_depends || pack->hard_depends) {
			/* only add this message if it's not going to be explained by a lower dependency */
			/* (avoids redundant info like "nautilus would not work anymore" -- DUH) */
			message = g_strdup_printf (_("%s requires the following :"), required);
			recoverable_error = TRUE;
		}
		break;
	case PACKAGE_BREAKS_DEPENDENCY:
		message = g_strdup_printf (_("%s would break other packages"), required);
		break;
	case PACKAGE_INVALID:
		break;
	case PACKAGE_CANNOT_OPEN:
		message = g_strdup_printf (_("%s is needed, but could not be found on Eazel's servers"), required);
		break;
	case PACKAGE_PARTLY_RESOLVED:
		break;
	case PACKAGE_ALREADY_INSTALLED:
		message = g_strdup_printf (_("%s was already installed"), required);
		break;
	case PACKAGE_CIRCULAR_DEPENDENCY: 
		message = g_strdup_printf (_("%s causes a circular dependency problem"), required);
		if (previous_pack->status == PACKAGE_CIRCULAR_DEPENDENCY) {
			g_warning ("%s and %s are mutexed!!", 
				   required_by,
				   required);
		} else {
			g_warning ("%s and %s is not the mutex", 
				   required_by,
				   required);
		}
		break;
	case PACKAGE_RESOLVED:
		recoverable_error = TRUE;	/* duh. */
		break;
	}
	g_free (required);
	g_free (required_by);

	if (! recoverable_error) {
		installer->all_errors_are_recoverable = FALSE;
	}

	if (message != NULL) {
		installer->failure_info = g_list_append (installer->failure_info, message);
	}

	/* Create the path list */
	data->path = g_list_prepend (data->path, pack);

	g_list_foreach (pack->soft_depends, (GFunc)get_detailed_errors_foreach, data);
	g_list_foreach (pack->hard_depends, (GFunc)get_detailed_errors_foreach, data);
	g_list_foreach (pack->modifies, (GFunc)get_detailed_errors_foreach, data);
	g_list_foreach (pack->breaks, (GFunc)get_detailed_errors_foreach, data);

	/* Pop the currect pack from the path */
	data->path = g_list_remove (data->path, pack);
}

static void
get_detailed_errors (const PackageData *pack, EazelInstaller *installer)
{
	GtkLabel *label_single;
	char *temp;
	GetErrorsForEachData data;
	PackageData *non_const_pack;

	LOG_DEBUG (("error tree traversal begins.\n"));
	installer->all_errors_are_recoverable = TRUE;
	installer->additional_packages = NULL;

	/* if a top-level package is already installed, it isn't really an "error" -- just a bug in the
	 * libeazelinstall impl. */
	if (pack->status == PACKAGE_ALREADY_INSTALLED) {
		label_single = gtk_object_get_data (GTK_OBJECT (installer->window), "label_overall");
		temp = g_strdup_printf (_("%s was already installed (skipping)"), pack->name);
		gtk_label_set_text (label_single, temp);
		g_free (temp);
		return;
	}

	data.installer = installer;
	data.path = NULL;
	non_const_pack = packagedata_copy (pack);
	get_detailed_errors_foreach (non_const_pack, &data);
	packagedata_destroy (non_const_pack, TRUE);
}

static void
install_failed (EazelInstall *service,
		const PackageData *pd,
		EazelInstaller *installer)
{
	LOG_DEBUG (("INSTALL FAILED.\n"));
	get_detailed_errors (pd, installer);

	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}
}

static void
download_failed (EazelInstall *service,
		 const char *name,
		 EazelInstaller *installer)
{
	char *temp;

	temp = g_strdup_printf (_("Download of %s failed"), name);
	installer->failure_info = g_list_append (installer->failure_info, temp);

	LOG_DEBUG (("Download FAILED for %s\n", name));
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

	label_single = gtk_object_get_data (GTK_OBJECT (installer->window), "label_single");
	label_single_2 = gtk_object_get_data (GTK_OBJECT (installer->window), "label_single_2");
	label_overall = gtk_object_get_data (GTK_OBJECT (installer->window), "label_overall");
	label_top = gtk_object_get_data (GTK_OBJECT (installer->window), "label_top");
	progress_single = gtk_object_get_data (GTK_OBJECT (installer->window), "progressbar_single");
	progress_overall = gtk_object_get_data (GTK_OBJECT (installer->window), "progressbar_overall");
	header_single = gtk_object_get_data (GTK_OBJECT (installer->window), "header_single");

	/* please wait for blah blah. */
	gtk_label_set_text (GTK_LABEL (label_top), WAIT_LABEL_2);

	/* change header from Download to Install */
	gtk_label_set_text (GTK_LABEL (header_single), _("Install Progress:"));
	gtk_label_set_text (GTK_LABEL (label_single), _("Preparing to install Nautilus and its dependencies"));
	gtk_label_set_text (GTK_LABEL (label_single_2), "");

	gtk_progress_set_percentage (GTK_PROGRESS (progress_single), 0.0);
	/* surprise!  we're 50% done now! */
	gtk_progress_configure (GTK_PROGRESS (progress_overall), 50.0, 0.0, 100.0);

	total_mb = (total_size + (512*1024)) / (1024*1024);
	if (num_packages == 1) {
		temp = g_strdup_printf (_("Installing 1 package (%d MB)"), total_mb);
	} else {
		temp = g_strdup_printf (_("Installing %d packages (%d MB)"), num_packages, total_mb);
	}
	gtk_label_set_text (GTK_LABEL (label_overall), temp);
	LOG_DEBUG (("PREFLIGHT: %s\n", temp));
	g_free (temp);

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
	GtkWidget *label_overall;
	char *temp;
	char *required = get_required_name (needs);

	label_overall = gtk_object_get_data (GTK_OBJECT (installer->window), "label_overall");
	/* careful: this needs->name is not always a package name (sometimes it's a filename) */
	temp = g_strdup_printf ("%s is required by %s", required, pack->name);
	gtk_label_set_text (GTK_LABEL (label_overall), temp);

	LOG_DEBUG (("DEP CHECK : %s\n", temp));

	g_free (temp);
	g_free (required);

	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}
}

static gboolean
eazel_install_delete_files (EazelInstall *service,
			    EazelInstaller *installer) 
{
	LOG_DEBUG (("Deleting rpm's\n"));
	if (installer->successful) {
		return TRUE;
	} else {
		return FALSE;
	}
}

static void
install_done (EazelInstall *service,
	      gboolean result,
	      EazelInstaller *installer)
{
	char *temp;

	installer->successful = result;
	LOG_DEBUG (("Done, result is %s\n", result ? "good" : "evil"));
	if (result) {
		gnome_druid_set_page (installer->druid, installer->finish_good);
	} else {
		/* will call jump_to_error_page later */
		if (installer->failure_info == NULL) {
			temp = g_strdup (_("Couldn't download or install the packages, for some reason"));
			installer->failure_info = g_list_append (installer->failure_info, temp);
		}
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
			LOG_DEBUG (("Invalid button for '%s'!\n", category->name));
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
	
	button = lookup_widget (installer->window, name);
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
toggle_button_toggled (GtkToggleButton *button,
		       EazelInstaller *installer) 
{
	GList *deps;
	GList *iterator;
	GList *item;

	LOG_DEBUG (("%s toggled to %s\n", gtk_widget_get_name (GTK_WIDGET (button)),
		    button->active ? "ACTIVE" : "deactivated"));

	item = g_list_find_custom (installer->categories, gtk_widget_get_name (GTK_WIDGET (button)),
				   (GCompareFunc)category_compare_func);
	if (item) {
		deps = ((CategoryData *)(item->data))->depends;
		for (iterator = deps; iterator; iterator = iterator->next) {
			toggle_button_lock (installer, 
					    (char*)iterator->data,
					    button->active);		
		}
	}

	check_if_next_okay (NULL, NULL, installer);
}

static void 
eazel_installer_add_category (EazelInstaller *installer,
			      CategoryData *category)
{
	GtkWidget *button;
	GtkWidget *vbox;
	gboolean render = TRUE;
	GtkWidget *label;
	GtkWidget *button_name;
	GtkWidget *hbox, *hbox2;
	char *temp;

	if (installer->debug) {
		fprintf (stdout, "Read category \"%s\"\n", category->name);
	}

	vbox = GTK_WIDGET (gtk_object_get_data (GTK_OBJECT (installer->window), "vbox3"));

	hbox = gtk_hbox_new (FALSE, 0);
	button = gtk_check_button_new ();
	button_name = gtk_label_new_with_font (category->name, FONT_NORM_BOLD);

	gtk_widget_show (button);
	gtk_widget_show (button_name);
	add_padding_to_box (hbox, 10, 0);
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

	label = gtk_label_new (category->description ? category->description : "");
	gtk_label_set_line_wrap (GTK_LABEL (label), FALSE);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	temp = g_strdup_printf ("%s/description", category->name);
	gtk_object_set_data_full (GTK_OBJECT (installer->window), temp, label,
				  (GtkDestroyNotify) gtk_widget_unref);
	g_free (temp);

	hbox2 = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox2), label, 0, 0, 40);
	gtk_widget_show (label);

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
		gtk_widget_show (hbox2);
		add_padding_to_box (vbox, 0, 10);
		gtk_box_pack_start (GTK_BOX (vbox), hbox, 0, 0, 0);
		gtk_box_pack_start (GTK_BOX (vbox), hbox2, 0, 0, 0);
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
	LOG_DEBUG (("host = %s\n", ub.nodename));
	if (!installer_test && g_strncasecmp (ub.nodename, "toothgnasher", 12)==0) {
		GnomeDialog *d;

		d = GNOME_DIALOG (gnome_warning_dialog_parented ("Eskil, din pattestive smølf.\n"
								 "Syn's du selv det går godt ? At\n"
								 "udføre denne installation på din egen\n"
								 "maskine er jo en kende dumt.\n"
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

	if (dist.name != DISTRO_REDHAT && !installer_test) {
		/* FIXME bugzilla.eazel.com
		   Find other distro's that use rpm */
		if (dist.name == DISTRO_MANDRAKE ||
		    dist.name == DISTRO_YELLOWDOG) {
			GnomeDialog *d;
			d = GNOME_DIALOG (gnome_warning_dialog_parented (_("You're running the installer on a"
									   "RPM-based system, but not a Red Hat"
									   "Linux release. I'll try it anyway."),
									 GTK_WINDOW (installer->window)));
			gnome_dialog_run_and_close (d);			
		} else {
			jump_to_error_page (installer, NULL,
					    _("Sorry, but this preview installer only works for RPM-based\n"
					      "systems.  You will have to download the source youself."), "");
			return FALSE;
		}
	}

	if (!installer_test && g_file_test ("/etc/eazel/profile/bashrc", G_FILE_TEST_ISFILE)) {
		jump_to_error_page (installer, NULL,
				    _("No, you've got the eazel-hacking environment.\n"
				      "You definitely do not want to run the installer\n"
				      "as it will screw up your system."), "");
		return FALSE;
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

/* returns TRUE if it's got more stuff to do */
gboolean
eazel_installer_do_install (EazelInstaller *installer, GList *install_categories)
{
       	GList *iter;
	GList *categories_copy;

	categories_copy = categorydata_list_copy (install_categories);
	eazel_install_install_packages (installer->service, categories_copy, NULL);
	/* now free this copy */
	categorydata_list_destroy (categories_copy);

/*
  FIXME: bugzilla.eazel.com 2604
	g_list_foreach (categories, (GFunc)categorydata_destroy_foreach, NULL);
	g_list_free (categories);
*/
	if (installer->failure_info != NULL) {
		if (installer->debug) {
			for (iter = g_list_first (installer->failure_info); iter != NULL; iter = g_list_next (iter)) {
				LOG_DEBUG (("ERROR : %s\n", (char *)(iter->data)));
			}
		}
		if (installer->all_errors_are_recoverable && (installer->additional_packages != NULL)) {
			jump_to_retry_page (installer);
			return TRUE;
		} else {
			jump_to_error_page (installer, installer->failure_info, ERROR_LABEL, ERROR_LABEL_2);
		}
	}
	return FALSE;
}


/*****************************************
  GTK+ object stuff
*****************************************/

static void
eazel_installer_finalize (GtkObject *object)
{
	EazelInstaller *installer;

	LOG_DEBUG (("eazel_installer_finalize\n"));

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
	eazel_install_unref (GTK_OBJECT (installer->service));
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

static gboolean
eazel_install_get_depends (EazelInstaller *installer, const char *dest_dir)
{
	char *url;
	char *destination;

	url = g_strdup_printf ("http://%s:%d/%s", 
			       eazel_install_get_server (installer->service),
			       eazel_install_get_server_port (installer->service),
			       PACKAGE_LIST);

	destination = g_strdup_printf ("%s/%s", dest_dir, PACKAGE_LIST);

	if (! eazel_install_fetch_file (installer->service, url, "package list", destination)) {
		/* try again with proxy config */
		unlink (destination);
		if (! attempt_http_proxy_autoconfigure () ||
		    ! eazel_install_fetch_file (installer->service, url, "package list", destination)) {
			jump_to_error_page (installer, NULL, ERROR_NEED_TO_SET_PROXY, "");
			rmdir (installer->tmpdir);
			return FALSE;
		}
	}

	g_free (destination);
	g_free (url);
	return TRUE;
}

static void
eazel_installer_initialize (EazelInstaller *object) {
	EazelInstaller *installer;
	GList *iterator;
	char *tmpdir;
	char *package_destination;
	char *splash_text = NULL;
	char *finish_text = NULL;
	int tries;
	GtkWidget *start_page;

	g_assert (object != NULL);
	g_assert (IS_EAZEL_INSTALLER (object));

	installer = EAZEL_INSTALLER (object);

	/* attempt to create a directory we can use */
#define RANDCHAR ('A' + (rand () % 23))
	srand (time (NULL));
	for (tries = 0; tries < 50; tries++) {
		tmpdir = g_strdup_printf ("/tmp/eazel-installer.%c%c%c%c%c%c%d",
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

	package_destination = g_strdup_printf ("%s/package-list.xml", tmpdir);

	installer->tmpdir = tmpdir;
	installer->test = installer_test;
	installer->debug = installer_debug;

	installer->must_have_categories = NULL;
	installer->implicit_must_have = NULL;
	installer->dont_show = NULL;

	installer->window = create_window (installer);

	gtk_widget_show (installer->window);
	if (! check_system (installer)) {
		return;
	}
	gnome_druid_set_buttons_sensitive (installer->druid, FALSE, FALSE, TRUE);

	/* show what we have so far */
	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}

	installer->failure_info = NULL;

	installer->service =  
		EAZEL_INSTALL (gtk_object_new (TYPE_EAZEL_INSTALL,
					       "verbose", TRUE,
					       "silent", FALSE,
					       "debug", TRUE,
					       "test", installer_test ? TRUE : FALSE, 
					       "force", installer_force ? TRUE : FALSE,
					       "depend", FALSE,
					       "update", TRUE,
					       "uninstall", FALSE,
					       "downgrade", FALSE,
					       "protocol", installer_local ? PROTOCOL_LOCAL: PROTOCOL_HTTP,
					       "tmp_dir", tmpdir,
					       "rpmrc_file", RPMRC,
					       "server", installer_server ? installer_server : HOSTNAME,
					       "server_port", 
					       installer_server_port ? installer_server_port : PORT_NUMBER,
					       "package_list", package_destination, 
					       "package_list_storage_path", "package-list.xml",
					       "transaction_dir", "/tmp",
					       "cgi_path", installer_cgi_path ? installer_cgi_path : CGI_PATH,
					       NULL));

	
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
			    "delete_files", 
			    GTK_SIGNAL_FUNC (eazel_install_delete_files), 
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
			    "done", 
			    GTK_SIGNAL_FUNC (install_done), 
			    installer);

	if (!installer->debug && 0) {
		char *log;
		log = g_strdup_printf ("%s/installer.log", tmpdir);
		eazel_install_open_log (installer->service, log);
		g_free (log);
	}

	/* now this also fetches the category deps too */
	if (! eazel_install_get_depends (installer, tmpdir)) {
		/* already posted error */
		return;
	}

	installer->categories = parse_local_xml_package_list (package_destination, &splash_text, &finish_text);

	if (!installer->categories) {
		CategoryData *cat = categorydata_new ();
		PackageData *pack = packagedata_new ();
		struct utsname ub;
		
		uname (&ub);
		g_warning ("Ugh, no categories");
		cat->name = g_strdup ("Nautilus");
		pack->name = g_strdup ("nautilus");
		pack->archtype = g_strdup (ub.machine);
		cat->packages = g_list_prepend (NULL, pack);
		installer->categories = g_list_prepend (NULL, cat);
	}

	for (iterator = installer->categories; iterator; iterator=iterator->next) {
#if 0
		/* eventually, it would be nice to go pre-fetch the list of required rpm's.  unfortunately
		 * the install lib isn't quite ready for that yet.
		 */
		eazel_install_fetch_definitive_category_info (installer->service, (CategoryData *)(iterator->data));
#endif
		eazel_installer_add_category (installer, (CategoryData*)iterator->data);
	}

	g_free (package_destination);

	if (splash_text != NULL) {
		start_page = gtk_object_get_data (GTK_OBJECT (installer->window), "start_page");
		nautilus_druid_page_eazel_set_text (NAUTILUS_DRUID_PAGE_EAZEL (start_page), "");
		if (1) {
			GtkWidget *title;
			GtkWidget *label;
			GtkWidget *vbox, *hbox1, *hbox2;

			/* put it in an hbox so it won't be indirectly centered */
			title = gtk_label_new_with_font (_("Welcome to the Eazel Installer!"), FONT_TITLE);
			gtk_label_set_justify (GTK_LABEL (title), GTK_JUSTIFY_LEFT);
			gtk_widget_show (title);
			hbox1 = gtk_hbox_new (FALSE, 0);
			gtk_box_pack_start (GTK_BOX (hbox1), title, FALSE, FALSE, 0);
			gtk_widget_show (hbox1);
			
			label = gtk_label_new (splash_text);
			gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
			gtk_widget_show (label);
			hbox2 = gtk_hbox_new (FALSE, 0);
			gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 20);
			gtk_widget_show (hbox2);

			vbox = gtk_vbox_new (FALSE, 0);
			gtk_box_pack_start (GTK_BOX (vbox), hbox1, FALSE, FALSE, 0);
			gtk_box_pack_start (GTK_BOX (vbox), hbox2, FALSE, FALSE, 0);
			gtk_widget_set_uposition (vbox, CONTENT_X, CONTENT_Y);
			gtk_widget_show (vbox);

			nautilus_druid_page_eazel_put_widget (NAUTILUS_DRUID_PAGE_EAZEL (start_page), vbox);
			gtk_widget_show (label);
		} else {		
			nautilus_druid_page_eazel_set_text (NAUTILUS_DRUID_PAGE_EAZEL (start_page), splash_text);
		}
		g_free (splash_text);
	}

	/* make good-finish page, now that we have the finish text for it */
	installer->finish_good = GNOME_DRUID_PAGE (create_finish_page_good (GTK_WIDGET (installer->druid),
									    installer->window,
									    finish_text));

	gtk_signal_connect (GTK_OBJECT (installer->finish_good), "prepare",
			    GTK_SIGNAL_FUNC (prep_finish),
			    installer);
	gtk_signal_connect (GTK_OBJECT (installer->finish_good), "finish",
			    GTK_SIGNAL_FUNC (druid_finish),
			    installer);

	gnome_druid_set_buttons_sensitive (installer->druid, FALSE, TRUE, TRUE);

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

