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
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/utsname.h>

#include "installer.h"
#include "callbacks.h"
#include "installer.h"
#include "support.h"


/* Include the pixmaps */
#include "Banner_Left.xpm"
#include "Step_Two_Top.xpm"
#include "Step_Three_Top.xpm"
#include "Step_One_Top.xpm"
#include "Final_Top.xpm"


#define EAZEL_SERVICES_DIR_HOME "/var/eazel"
#define EAZEL_SERVICES_DIR EAZEL_SERVICES_DIR_HOME "/services"

#define HOSTNAME "testmachine.eazel.com"
#define PORT_NUMBER 80
#define TMP_DIR "/tmp/eazel-install"
#define RPMRC "/usr/lib/rpm/rpmrc"
#define REMOTE_RPM_DIR "/RPMS"

int installer_debug = 0;
int installer_output = 0;
int installer_test = 0;
int installer_force = 0;
int installer_local = 0;
char *installer_server =NULL;
int installer_server_port = 0;


static GtkObjectClass *eazel_installer_parent_class;


static void 
set_white_stuff (GtkWidget *w) 
{
	GtkStyle *style;
	GdkColor *color;

	style = gtk_style_copy (w->style);
	style->bg[GTK_STATE_NORMAL].red = 65000;
	style->bg[GTK_STATE_NORMAL].blue = 65000;
	style->bg[GTK_STATE_NORMAL].green = 65000;
	gtk_widget_set_style (w, style);
        gtk_style_unref (style);
}

GtkWidget*
create_what_to_do_page (GtkWidget *druid, GtkWidget *window)
{
	GtkWidget *what_to_do_page;
	GdkColor what_to_do_page_bg_color = { 0, 3341, 23130, 26214 };
	GdkColor what_to_do_page_logo_bg_color = { 0, 3341, 23130, 26214 };
	GdkColor what_to_do_page_title_color = { 0, 65535, 65535, 65535 };
	GtkWidget *druid_vbox1;
	GtkWidget *vbox3;
	GtkWidget *label10;
	GtkWidget *fixed3;

	what_to_do_page = gnome_druid_page_standard_new_with_vals ("", NULL);

	set_white_stuff (GTK_WIDGET (what_to_do_page));

	gtk_widget_set_name (what_to_do_page, "what_to_do_page");
	gtk_widget_ref (what_to_do_page);
	gtk_object_set_data_full (GTK_OBJECT (window), "what_to_do_page", what_to_do_page,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show_all (what_to_do_page);
	gnome_druid_append_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (what_to_do_page));
	gnome_druid_page_standard_set_bg_color (GNOME_DRUID_PAGE_STANDARD (what_to_do_page), 
						&what_to_do_page_bg_color);
	gnome_druid_page_standard_set_logo_bg_color (GNOME_DRUID_PAGE_STANDARD (what_to_do_page), 
						     &what_to_do_page_logo_bg_color);
	gnome_druid_page_standard_set_title_color (GNOME_DRUID_PAGE_STANDARD (what_to_do_page), 
						   &what_to_do_page_title_color);
	gnome_druid_page_standard_set_title (GNOME_DRUID_PAGE_STANDARD (what_to_do_page), _("What to do ?"));

	druid_vbox1 = GNOME_DRUID_PAGE_STANDARD (what_to_do_page)->vbox;
	set_white_stuff (GTK_WIDGET (druid_vbox1));
	gtk_widget_set_name (druid_vbox1, "druid_vbox1");
	gtk_widget_ref (druid_vbox1);
	gtk_object_set_data_full (GTK_OBJECT (window), "druid_vbox1", druid_vbox1,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (druid_vbox1);

	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_set_name (vbox3, "vbox3");
	gtk_widget_ref (vbox3);
	gtk_object_set_data_full (GTK_OBJECT (window), "vbox3", vbox3,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (vbox3);
	gtk_box_pack_start (GTK_BOX (druid_vbox1), vbox3, TRUE, TRUE, 0);

	label10 = gtk_label_new (_("You have several choices for what you would like the installer to do.\n"
				   "Please choose one and click on the \"Next\" button to begin install."));
	gtk_widget_set_name (label10, "label10");
	gtk_widget_ref (label10);
	gtk_object_set_data_full (GTK_OBJECT (window), "label10", label10,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label10);
	gtk_box_pack_start (GTK_BOX (vbox3), label10, FALSE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (label10), GTK_JUSTIFY_LEFT);

	fixed3 = gtk_fixed_new ();
	set_white_stuff (GTK_WIDGET (fixed3));
	gtk_widget_set_name (fixed3, "fixed3");
	gtk_widget_ref (fixed3);
	gtk_object_set_data_full (GTK_OBJECT (window), "fixed3", fixed3,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (fixed3);
	gtk_box_pack_start (GTK_BOX (vbox3), fixed3, TRUE, TRUE, 0);

	return what_to_do_page;
}

GtkWidget*
create_install_page (GtkWidget *druid, GtkWidget *window)
{
	GtkWidget *install_page;
	GdkColor install_page_bg_color = { 0, 3341, 23130, 26214 };
	GdkColor install_page_logo_bg_color = { 0, 3341, 23130, 26214 };
	GdkColor install_page_title_color = { 0, 65535, 65535, 65535 };
	GtkWidget *druid_vbox2;
	GtkWidget *vbox5;
	GtkWidget *label11;
	GtkWidget *table2;
	GtkWidget *label12;
	GtkWidget *label13;
	GtkWidget *action_label;
	GtkWidget *progressbar1;
	GtkWidget *progressbar2;
	GtkWidget *package_label;
	GtkWidget *fixed1;
	GtkWidget *textbox;
	GtkWidget *scrolledwindow;
	const char *download_description;
	int download_description_length;
	
	download_description = g_strdup (_("Currently downloading packages required to "
					   "install Nautilus\n"));
	download_description_length = strlen (download_description);

	install_page = gnome_druid_page_standard_new_with_vals ("", NULL);
	set_white_stuff (GTK_WIDGET (install_page));
	gtk_widget_set_name (install_page, "install_page");
	gtk_widget_ref (install_page);
	gtk_object_set_data_full (GTK_OBJECT (window), "install_page", install_page,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show_all (install_page);
	gnome_druid_append_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (install_page));
	gnome_druid_page_standard_set_bg_color (GNOME_DRUID_PAGE_STANDARD (install_page), 
						&install_page_bg_color);
	gnome_druid_page_standard_set_logo_bg_color (GNOME_DRUID_PAGE_STANDARD (install_page), 
						     &install_page_logo_bg_color);
	gnome_druid_page_standard_set_title_color (GNOME_DRUID_PAGE_STANDARD (install_page), 
						   &install_page_title_color);
	gnome_druid_page_standard_set_title (GNOME_DRUID_PAGE_STANDARD (install_page), _("Progress..."));

	druid_vbox2 = GNOME_DRUID_PAGE_STANDARD (install_page)->vbox;
	gtk_widget_set_name (druid_vbox2, "druid_vbox2");
	gtk_widget_ref (druid_vbox2);
	gtk_object_set_data_full (GTK_OBJECT (window), "druid_vbox2", druid_vbox2,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (druid_vbox2);

	vbox5 = gtk_vbox_new (FALSE, 0);
	set_white_stuff (GTK_WIDGET (vbox5));
	gtk_widget_set_name (vbox5, "vbox5");
	gtk_widget_ref (vbox5);
	gtk_object_set_data_full (GTK_OBJECT (window), "vbox5", vbox5,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (vbox5);
	gtk_box_pack_start (GTK_BOX (druid_vbox2), vbox5, TRUE, TRUE, 0);

	table2 = gtk_table_new (3, 2, FALSE);
	set_white_stuff (GTK_WIDGET (table2));
	gtk_widget_set_name (table2, "table2");
	gtk_widget_ref (table2);
	gtk_object_set_data_full (GTK_OBJECT (window), "table2", table2,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (table2);
	gtk_table_set_row_spacings (GTK_TABLE (table2), 16);
	gtk_box_pack_start (GTK_BOX (vbox5), table2, FALSE, FALSE, 16);

	progressbar1 = gtk_progress_bar_new ();
	gtk_widget_set_name (progressbar1, "progressbar_single");
	gtk_widget_ref (progressbar1);
	gtk_object_set_data_full (GTK_OBJECT (window), "progressbar_single", progressbar1,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_progress_set_show_text (GTK_PROGRESS (progressbar1), TRUE);		  
	gtk_widget_show (progressbar1);
	gtk_table_attach (GTK_TABLE (table2), progressbar1, 1, 2, 1, 2,
			  /* GTK_EXPAND */ 0,
			  /* GTK_EXPAND */ GTK_SHRINK,
			  0, 0);

	progressbar2 = gtk_progress_bar_new ();
	gtk_widget_set_name (progressbar2, "progressbar_overall");
	gtk_widget_ref (progressbar2);
	gtk_object_set_data_full (GTK_OBJECT (window), "progressbar_overall", progressbar2,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_progress_set_format_string (GTK_PROGRESS (progressbar2), "Waiting for download...");
	gtk_progress_set_show_text (GTK_PROGRESS (progressbar2), TRUE);		  
	gtk_widget_show (progressbar2); 
	gtk_table_attach (GTK_TABLE (table2), progressbar2, 1, 2, 2, 3,
			  /* GTK_EXPAND */ 0,
			  /* GTK_EXPAND */ GTK_SHRINK,
			  0, 0);

	package_label = gtk_label_new (_("En fjæsing hedder Bent"));
	gtk_widget_set_name (package_label, "package_label");
	gtk_widget_ref (package_label);
	gtk_object_set_data_full (GTK_OBJECT (window), "package_label", package_label,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (package_label);
	gtk_table_attach (GTK_TABLE (table2), package_label, 1, 2, 0, 1,
			  GTK_EXPAND,
			  GTK_EXPAND,
			  0, 0);
/*
	fixed1 = gtk_fixed_new ();
	gtk_widget_set_name (fixed1, "fixed1");
	set_white_stuff (GTK_WIDGET (fixed1));
	gtk_widget_ref (fixed1);
	gtk_object_set_data_full (GTK_OBJECT (window), "fixed1", fixed1,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (fixed1);
	gtk_box_pack_start (GTK_BOX (vbox5), fixed1, TRUE, TRUE, 16);	
*/


	textbox = gtk_label_new ("");
	gtk_widget_set_name (textbox, "textbox");
	gtk_widget_ref (textbox);
	gtk_object_set_data_full (GTK_OBJECT (window), "summary", textbox,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (textbox);
	gtk_label_set_text (GTK_LABEL (textbox), download_description);
	gtk_box_pack_start (GTK_BOX (vbox5), textbox, TRUE, TRUE, 0);	

	return install_page;
}

GtkWidget*
create_finish_page (GtkWidget *druid, GtkWidget *window)
{
	GtkWidget *finish_page;
	GdkColor finish_page_bg_color = { 0, 3341, 23130, 26214 };
	GdkColor finish_page_textbox_color = { 0, 65535, 65535, 65535 };
	GdkColor finish_page_logo_bg_color = { 0, 3341, 23130, 26214 };
	GdkColor finish_page_title_color = { 0, 65535, 65535, 65535 };

	finish_page = gnome_druid_page_finish_new ();
	gtk_widget_set_name (finish_page, "finish_page");
	gtk_widget_ref (finish_page);
	gtk_object_set_data_full (GTK_OBJECT (window), "finish_page", finish_page,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (finish_page);
	gnome_druid_append_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (finish_page));
	gnome_druid_page_finish_set_bg_color (GNOME_DRUID_PAGE_FINISH (finish_page), 
					      &finish_page_bg_color);
	gnome_druid_page_finish_set_textbox_color (GNOME_DRUID_PAGE_FINISH (finish_page), 
						   &finish_page_textbox_color);
	gnome_druid_page_finish_set_logo_bg_color (GNOME_DRUID_PAGE_FINISH (finish_page), 
						   &finish_page_logo_bg_color);
	gnome_druid_page_finish_set_title_color (GNOME_DRUID_PAGE_FINISH (finish_page), 
						 &finish_page_title_color);
	gnome_druid_page_finish_set_title (GNOME_DRUID_PAGE_FINISH (finish_page), _("Finished"));
	gnome_druid_page_finish_set_text (GNOME_DRUID_PAGE_FINISH (finish_page), 
					  _("If the installation was successfull, you can\n"
					    "find the nautilus icon in the applications menu.\n\n"
					    "Thanks for taking the time to try out Nautilus.\n\n"
					    "May your life be a healthy and happy one."));

	return finish_page;
}

GtkWidget*
create_window (EazelInstaller *installer)
{
	GtkWidget *window;
	GtkWidget *druid;
	GtkWidget *start_page;
	GdkColor start_page_bg_color = { 0, 3341, 23130, 26214 };
	GdkColor start_page_textbox_color = { 0, 65535, 65535, 65535 };
	GdkColor start_page_logo_bg_color = { 0, 3341, 23130, 26214 };
	GdkColor start_page_title_color = { 0, 65535, 65535, 65535 };
	GtkWidget *what_to_do_page;
	GtkWidget *install_page;
	GtkWidget *finish_page;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_name (window, "window");
	gtk_object_set_data (GTK_OBJECT (window), "window", window);
	gtk_window_set_title (GTK_WINDOW (window), _("Nautilus install tool"));

	druid = gnome_druid_new ();
	gtk_widget_set_name (druid, "druid");
	gtk_widget_ref (druid);
	gtk_object_set_data_full (GTK_OBJECT (window), "druid", druid,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (druid);
	gtk_container_add (GTK_CONTAINER (window), druid);

	start_page = gnome_druid_page_start_new ();
	gtk_widget_set_name (start_page, "start_page");
	gtk_widget_ref (start_page);
	gtk_object_set_data_full (GTK_OBJECT (window), "start_page", start_page,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (start_page);
	gnome_druid_append_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (start_page));
	gnome_druid_set_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (start_page));
	gnome_druid_page_start_set_bg_color (GNOME_DRUID_PAGE_START (start_page), 
					     &start_page_bg_color);
	gnome_druid_page_start_set_textbox_color (GNOME_DRUID_PAGE_START (start_page), 
						  &start_page_textbox_color);
	gnome_druid_page_start_set_logo_bg_color (GNOME_DRUID_PAGE_START (start_page), 
						  &start_page_logo_bg_color);
	gnome_druid_page_start_set_title_color (GNOME_DRUID_PAGE_START (start_page), 
						&start_page_title_color);
	gnome_druid_page_start_set_title (GNOME_DRUID_PAGE_START (start_page), _("Step one:"));
	gnome_druid_page_start_set_text (GNOME_DRUID_PAGE_START (start_page), 
					 _("This is the internal Nautilus installer.\n\n"
					   "Lots of text should go here letting you know what you need\n"
					   "to have installed before you should even begin to think about\n"
					   "using this. For example:\n"
					   "\n"
					   "  * Stuff\n"
					   "  * More stuff\n"
					   "  * Other stuff\n"
					   "\n"
					   "If you meet these requirements, hit the \"Next\" button to continue!\n\n"));

	what_to_do_page = create_what_to_do_page (druid, window);
	install_page = create_install_page (druid, window);
	finish_page = create_finish_page (druid, window);

	gtk_signal_connect (GTK_OBJECT (druid), "cancel",
			    GTK_SIGNAL_FUNC (druid_cancel),
			    installer);
	gtk_signal_connect (GTK_OBJECT (install_page), "finish",
			    GTK_SIGNAL_FUNC (druid_finish),
			    installer);
	gtk_signal_connect (GTK_OBJECT (install_page), "prepare",
			    GTK_SIGNAL_FUNC (prep_install),
			    installer);
	gtk_signal_connect (GTK_OBJECT (finish_page), "finish",
			    GTK_SIGNAL_FUNC (druid_finish),
			    installer);

	return window;
}

static void
set_images  (GtkWidget *window)
{
	
	GnomeDruidPage *page;

	page = GNOME_DRUID_PAGE (gtk_object_get_data(GTK_OBJECT (window), "start_page"));
	gnome_druid_page_start_set_logo (GNOME_DRUID_PAGE_START (page), 
					 gdk_imlib_create_image_from_xpm_data (step_one_top));
	gnome_druid_page_start_set_watermark (GNOME_DRUID_PAGE_START (page), 
					      gdk_imlib_create_image_from_xpm_data (banner_left));

	page = GNOME_DRUID_PAGE (gtk_object_get_data(GTK_OBJECT (window), "what_to_do_page"));
	gnome_druid_page_standard_set_logo (GNOME_DRUID_PAGE_STANDARD (page), 
					    gdk_imlib_create_image_from_xpm_data (step_two_top));

	page = GNOME_DRUID_PAGE (gtk_object_get_data(GTK_OBJECT (window), "install_page"));
	gnome_druid_page_standard_set_logo (GNOME_DRUID_PAGE_STANDARD (page), 
					    gdk_imlib_create_image_from_xpm_data (step_three_top));

	page = GNOME_DRUID_PAGE (gtk_object_get_data(GTK_OBJECT (window), "finish_page"));
	gnome_druid_page_finish_set_logo (GNOME_DRUID_PAGE_FINISH (page), 
					  gdk_imlib_create_image_from_xpm_data (final_top));
	gnome_druid_page_finish_set_watermark (GNOME_DRUID_PAGE_FINISH (page), 
					       gdk_imlib_create_image_from_xpm_data (banner_left));
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
	GtkWidget *summary;
	GtkLabel *package_label;

	package_label = gtk_object_get_data (GTK_OBJECT (installer->window), "package_label");
	summary = gtk_object_get_data (GTK_OBJECT (installer->window), "summary");
	progressbar = gtk_object_get_data (GTK_OBJECT (installer->window), "progressbar_single");
	progress_overall = gtk_object_get_data (GTK_OBJECT (installer->window), "progressbar_overall");

	if (amount == 0) {
		char *tmp;
		tmp = g_strdup_printf ("Installing %s", package->name);
		gtk_label_set_text (package_label, tmp);
		g_free (tmp);

		gtk_progress_set_format_string (GTK_PROGRESS (progressbar), "%p%% (%v of %u kb)");
		gtk_progress_configure (GTK_PROGRESS (progressbar), 0, 0, (float)(total/1024));		

		gtk_label_set_text (GTK_LABEL (summary), package->description);
		fprintf (stdout, "\n");
	}

	if (installer_output) {
		float pct;
		pct = ( (total > 0) ? ((float) ((((float) amount) / total) * 100)): 100.0);
		fprintf (stdout, "Install Progress - %s - %d %d (%d %d) %% %f\r", 
			 package->name?package->name:"(null)", 
			 amount, total, 
			 total_size_completed, total_size, 
			 pct);
	}

	gtk_progress_set_value (GTK_PROGRESS (progressbar), 
				(float)(amount/1024 > total/1024 ? total/1024 : amount/1024));
	gtk_progress_set_value (GTK_PROGRESS (progress_overall), 
				(float)total_size_completed>total_size ? total_size : total_size_completed);

	fflush (stdout);
	if (amount == total && installer_output) {
		fprintf (stdout, "\n");
	}
}


static void 
eazel_download_progress (EazelInstall *service, 
			 const char *name,
			 int amount, 
			 int total,
			 EazelInstaller *installer) 
{
	GtkProgressBar *progressbar;
	GtkLabel *package_label;

	package_label = gtk_object_get_data (GTK_OBJECT (installer->window), "package_label");
	progressbar = gtk_object_get_data (GTK_OBJECT (installer->window), "progressbar_single");

	if (amount == 0) {
		char *tmp;
		tmp = g_strdup_printf ("Retrieving %s", name);
		gtk_label_set_text (package_label, tmp);
		g_free (tmp);

		gtk_progress_set_format_string (GTK_PROGRESS (progressbar), "%p%% (%v of %u kb)");
		gtk_progress_configure (GTK_PROGRESS (progressbar), 0, 0, (float)(total/1024));
	}

	if (installer_output) {
		float pct;
		pct = ( (total > 0) ? ((float) ((((float) amount) / total) * 100)): 100.0);
		fprintf (stdout, "DOWNLOAD Progress - %s - %d %d %% %f\r", 
			 name?name:"(null)", amount, total, pct); 
		fflush (stdout);
	}
	
	gtk_progress_set_value (GTK_PROGRESS (progressbar), amount/1024);

	if (amount != total) {		
		/* g_main_iteration (FALSE); */
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
		tmp = g_strdup_printf ("%s\n***The package %s failed. Here's the dep tree\n", 
				       (*str)?*str:"", pd->name ? pd->name : pd->filename);
		g_free (*str);
		(*str) = tmp;
	}
	switch (pd->status) {
	case PACKAGE_DEPENDENCY_FAIL: {
		char *tmp;
		tmp = g_strdup_printf ("%s%s-%s failed\n", 
				       (*str)?*str:"", indent, pd->name ? pd->name : pd->filename);
		g_free (*str);
		(*str) = tmp;
		break;
	}
	case PACKAGE_CANNOT_OPEN: {
		char *tmp;
		tmp = g_strdup_printf ("%s%s-%s NOT FOUND\n", 
				       (*str)?*str:"", indent, pd->name ? pd->name : pd->filename);
		g_free (*str);
		(*str) = tmp;
		break;		
	}
	case PACKAGE_SOURCE_NOT_SUPPORTED: {
		char *tmp;
		tmp = g_strdup_printf ("%s%s-%s is a source\n", 
				       (*str)?*str:"", indent, pd->name ? pd->name : pd->filename);
		g_free (*str);
		(*str) = tmp;
		break;
	}
	case PACKAGE_BREAKS_DEPENDENCY: {
		char *tmp;
		tmp = g_strdup_printf ("%s%s-%s breaks\n", 
				       (*str)?*str:"", indent, pd->name ? pd->name : pd->filename);
		g_free (*str);
		(*str) = tmp;
		break;
	}
	default: {
		char *tmp;
		tmp = g_strdup_printf ("%s%s-%s\n", 
				       (*str)?*str:"", indent, pd->name ? pd->name : pd->filename);
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
		EazelInstaller *installer)
{
	if (pd->toplevel == TRUE) {
		install_failed_helper (service, pd, g_strdup (""), &installer->failure_info);
	}
}

static void
download_failed (EazelInstall *service,
		 const char *name,
		 EazelInstaller *installer)
{
	char *output;

	if (output) {		
		output = g_strdup_printf ("%s\nDownload of %s failed", installer->failure_info, name);
	} else {
		output = g_strdup_printf ("Download of %s failed", name);
	}
	g_free (installer->failure_info);
	installer->failure_info = output;
}

static void
eazel_install_preflight (EazelInstall *service,
			 int total_size,
			 int num_packages,
			 EazelInstaller *installer)
{
	GtkProgressBar *progress_overall;
	GtkLabel *package_label;
	GtkWidget *summary;
	char *summary_string;
	char *tmp;

	summary = gtk_object_get_data (GTK_OBJECT (installer->window), "summary");
	package_label = gtk_object_get_data (GTK_OBJECT (installer->window), "package_label");
	progress_overall = gtk_object_get_data (GTK_OBJECT (installer->window), "progressbar_overall");

	gtk_progress_set_format_string (GTK_PROGRESS (progress_overall), "Total completion %p%%");
	gtk_progress_configure (GTK_PROGRESS (progress_overall), 0, 0, (float)total_size);
	gtk_widget_show (GTK_WIDGET (progress_overall));

	summary_string = g_strdup_printf (_("Now starting the install process.\n"
					    "Starting the process takes some time, please be patient.\n"
					    "In total, %d mb of software will be installed"), 
					  total_size/(1024*1024));
	tmp = g_strdup_printf ("Preparing RPM, %d packages (%d mb)", num_packages, total_size/(1024*1024));

	if (installer_output) {
		fprintf (stdout, "PREFLIGHT: %s\n", tmp);
	}

	gtk_label_set_text (package_label, tmp);
	gtk_label_set_text (GTK_LABEL (summary), summary_string);
	g_main_iteration (FALSE);
}

static void
eazel_install_dep_check (EazelInstall *service,
			 const PackageData *pack,
			 const PackageData *needs,
			 EazelInstaller *installer)
{
	GtkLabel *package_label;
	GtkWidget *summary;
	char *tmp;

	package_label = gtk_object_get_data (GTK_OBJECT (installer->window), "package_label");
	summary = gtk_object_get_data (GTK_OBJECT (installer->window), "summary");

	tmp = g_strdup_printf ("%s needs %s", pack->name, needs->name);

	if (installer_output) {
		fprintf (stdout, "DEP CHECK : %s\n", tmp);
	}

	gtk_label_set_text (package_label, tmp);
	g_free (tmp);
	tmp = g_strdup_printf ("Fetching dependencies for %s", pack->name);
		
	gtk_label_set_text (GTK_LABEL (summary), tmp);
	g_free (tmp);

	g_main_iteration (FALSE);
}

static gboolean
eazel_install_delete_files (EazelInstall *service,
			    EazelInstaller *installer) 
{
	if (installer_output) {
		fprintf (stdout, "Deleting rpm's\n");
	}
	return TRUE ;
}

static void 
eazel_installer_add_category (EazelInstaller *installer,
			      CategoryData *category)
{
	static int magic = 24;
	GtkWidget *button;
	GtkWidget *fixed;
	static GSList *fixed_group = NULL;

	if (installer->debug) {
		fprintf (stdout, "Read category \"%s\"\n", category->name);
	}

	fixed = GTK_WIDGET (gtk_object_get_data (GTK_OBJECT (installer->window), "fixed3"));

	button = gtk_check_button_new_with_label (category->name);
	gtk_widget_set_name (button, category->name);
	gtk_widget_ref (button);
	gtk_object_set_data_full (GTK_OBJECT (installer->window), category->name, button,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (button);
	gtk_fixed_put (GTK_FIXED (fixed), button, 72, magic);
	gtk_widget_set_uposition (button, 72, magic);
	gtk_widget_set_usize (button, 0, 0);

	magic += 32;
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
					g_error (_("*** Could not create services directory (%s)! ***\n"), 
						 EAZEL_SERVICES_DIR_HOME);
				}
			}
		}

		retval = mkdir (EAZEL_SERVICES_DIR, 0755);
		if (retval < 0) {
			if (errno != EEXIST) {
				g_error (_("*** Could not create services directory (%s)! ***\n"), 
					 EAZEL_SERVICES_DIR);
			}
		}
	}
}

void
check_system (EazelInstaller *installer)
{
	DistributionInfo dist;
	struct utsname ub;

	dist = trilobite_get_distribution ();

#ifndef NAUTILUS_INSTALLER_RELEASE
	uname (&ub);
	/* This codes tells Eskil that he's an idiot if he runs it on his own machine
	   without the testflag... */
	g_message ("host = %s", ub.nodename);
	if (!installer_test && g_strncasecmp (ub.nodename, "toothgnasher", 12)==0) {
		GnomeDialog *d;
		d = GNOME_DIALOG (gnome_warning_dialog_parented ("Eskil, din nar. Du må aldrig nogensinde\n"
								 "udføre denne installation på din egen\n"
								 "maskine! Den smadrer jo alt!\n"
								 "Jeg slår lige --test til...",
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
		GnomeDialog *d;
		d = GNOME_DIALOG (gnome_warning_dialog_parented (_("This preview installer only works\n"
								   "for RPM based systems. You will have\n"
								   "to download the source yourself."),
								 GTK_WINDOW (installer->window)));
		gnome_dialog_run_and_close (d);
		exit (1);
	}

	if (!g_file_test ("/etc/pam.d/helix-update", G_FILE_TEST_ISFILE)) {
		GnomeDialog *d;
		d = GNOME_DIALOG (gnome_warning_dialog_parented (_("You do not have HelixCode gnome installed.\n"
								   "This means I will install the required parts\n"
								   "for you, but you might want to abort the\n"
								   "installer and go to http://www.helixcode.com\n"
								   "and download the full HelixCode Gnome\n"
								   "installation"),
								 GTK_WINDOW (installer->window)));
		gnome_dialog_run_and_close (d);
	}
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

void 
eazel_installer_do_install (EazelInstaller *installer,
			    GList *categories)
{
       	
	eazel_install_install_packages (installer->service, categories, NULL);
#if 0
	revert_nautilus_install (service, NULL);
#endif
/*
  FIXME: bugzilla.eazel.com 2604
	g_list_foreach (categories, (GFunc)categorydata_destroy_foreach, NULL);
	g_list_free (categories);
*/
	if (installer->failure_info && strlen (installer->failure_info)>1) {
		if (installer->debug) {
			fprintf (stdout, "ERROR :\n%s", installer->failure_info);
		}
		gnome_error_dialog_parented (installer->failure_info, GTK_WINDOW (installer->window));
	} else {
	}
}



/*****************************************
  GTK+ object stuff
*****************************************/

static void
eazel_installer_finalize (GtkObject *object)
{
	EazelInstaller *installer;

	g_message ("eazel_installer_finalize");

	g_return_if_fail (object != NULL);
	g_return_if_fail (EAZEL_INSTALLER (object));

	installer = EAZEL_INSTALLER (object);

	/* Free the objects own crap */
	g_list_foreach (installer->categories, (GFunc)categorydata_destroy_foreach, NULL);
	g_list_free (installer->categories);
	eazel_install_unref (GTK_OBJECT (installer->service));

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
eazel_installer_initialize (EazelInstaller *object) {
	EazelInstaller *installer;
	GList *iterator;
	char *tmpdir;
	char *package_destination;

	g_assert (object != NULL);
	g_assert (IS_EAZEL_INSTALLER (object));

	installer = EAZEL_INSTALLER (object);

	/* The manpage says not to use this, but I can't be arsed */
	tmpdir = g_strdup ("/tmp/eazel-installer.XXXXXX");
	mktemp (tmpdir);

	if (!tmpdir || mkdir (tmpdir, 0700) != 0) {
		g_error (_("Cannot create %s"), tmpdir);
	}

	package_destination = g_strdup_printf ("%s/package-list.xml", tmpdir);

	installer->test = installer_test;
	installer->debug = installer_debug;
	installer->output = installer_output;
	installer->category_deps = g_hash_table_new (g_str_hash, g_str_equal);
	installer->window = create_window (installer);
	set_images (installer->window);
	check_system (installer);
	gtk_widget_show (installer->window);

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
					       "downgrade", TRUE,
					       "protocol", installer_local ? PROTOCOL_LOCAL: PROTOCOL_HTTP,
					       "tmp_dir", tmpdir,
					       "rpmrc_file", RPMRC,
					       "server", installer_server ? installer_server : HOSTNAME,
					       "server_port", 
					       installer_server_port ? installer_server_port : PORT_NUMBER,
					       "package_list", package_destination, 
					       "package_list_storage_path", "package-list.xml",
					       "transaction_dir", "/tmp",
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

	if (!installer->debug) {
		char *log;
		log = g_strdup_printf ("%s/installer.log", tmpdir);
		eazel_install_open_log (installer->service, log);
		g_free (log);
	}

	eazel_install_fetch_remote_package_list (installer->service);
	installer->categories = parse_local_xml_package_list (package_destination);
	for (iterator = installer->categories; iterator; iterator=iterator->next) {
		eazel_installer_add_category (installer, (CategoryData*)iterator->data);
	}
	g_free (package_destination);
	g_free (tmpdir);
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
EazelInstaller*
eazel_installer_new()
{
	EazelInstaller *installer;

	installer = EAZEL_INSTALLER (gtk_object_new (TYPE_EAZEL_INSTALLER, NULL));
	
	return installer;
}
