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
#include "callbacks.h"
#include "support.h"
#include "installer.h"



static void
ask_to_delete_rpms (EazelInstaller *installer)
{
	GtkWidget *toplevel, *dialog;
	char *message;
	char *package_list;
	gboolean answer;

	message = g_strdup_printf (_("Would you like to delete the downloaded package files?\n"
				     "(They are no longer needed by the installer.)\n\n"
				     "The package files are stored in %s"), installer->tmpdir);
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (installer->druid));
	if (GTK_IS_WINDOW (toplevel)) {
		dialog = gnome_question_dialog_parented (message, (GnomeReplyCallback)gnome_reply_callback,
							 &answer, GTK_WINDOW (toplevel));
	} else {
		dialog = gnome_question_dialog (message, (GnomeReplyCallback)gnome_reply_callback, &answer);
	}
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));

	if (answer) {
		g_message ("Deleting package files...");
		package_list = g_strdup_printf ("%s/package-list.xml", installer->tmpdir);
		unlink (package_list);
		g_free (package_list);
		eazel_install_delete_downloads (installer->service);
	}
}

static gboolean
ask_are_you_sure (EazelInstaller *installer)
{
	GtkWidget *toplevel, *dialog;
	char *message;
	gboolean answer;

	message = _("Cancel the installation:\nAre you sure?");
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (installer->druid));
	if (GTK_IS_WINDOW (toplevel)) {
		dialog = gnome_question_dialog_parented (message, (GnomeReplyCallback)gnome_reply_callback,
							 &answer, GTK_WINDOW (toplevel));
	} else {
		dialog = gnome_question_dialog (message, (GnomeReplyCallback)gnome_reply_callback, &answer);
	}
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	return answer;
}

void
druid_cancel (GnomeDruid      *gnomedruid,
	      EazelInstaller  *installer)
{
	g_mem_profile ();
	if (installer != NULL) {
		if (ask_are_you_sure (installer)) {
			gtk_object_unref (GTK_OBJECT (installer));
			exit (1);
		}
	}
}

void
druid_delete (GtkWidget *widget, GdkEvent *event, EazelInstaller *installer)
{
	if (installer != NULL) {
		gtk_object_unref (GTK_OBJECT (installer));
	}
	g_mem_profile ();
	exit (1);
}

gboolean
begin_install (EazelInstaller  *installer)
{
	GtkWidget *window = installer->window;
	GnomeDruid *druid;

	druid = GNOME_DRUID (gtk_object_get_data (GTK_OBJECT (window), "druid"));

	gnome_druid_set_buttons_sensitive(druid, FALSE, FALSE, TRUE);

	g_message ("dep-check:%s install-categories:%s problems:%s successful:%s", 
		   installer->got_dep_check ? "TRUE" : "FALSE",
		   installer->install_categories ? "TRUE" : "FALSE",
		   installer->problems ? "TRUE" : "FALSE",
		   installer->successful ? "TRUE" : "FALSE");

	/* First time ? If so, check which categories were marked */
	if (installer->got_dep_check==FALSE && installer->install_categories == NULL) {
		GList *iterator;
		GList *install_categories = NULL;
		log_debug ("first time through");
		for (iterator = installer->categories; iterator; iterator = iterator->next) {
			CategoryData *category = (CategoryData*)iterator->data;
			GtkWidget *widget = gtk_object_get_data (GTK_OBJECT (window), category->name);
			
			/* widget will be NULL for made-up categories created by a 2nd attempt */
			if ((widget == NULL) || GTK_TOGGLE_BUTTON (widget)->active) {
				install_categories = g_list_append (install_categories, category);
			}
		}
		installer->install_categories = install_categories;
	} 
	
	if (installer->successful && installer->force_remove_categories) { 
		log_debug ("-> force remove categories");
		eazel_installer_do_install (installer, installer->force_remove_categories, TRUE);
		eazel_installer_post_install (installer);
		categorydata_list_destroy (installer->force_remove_categories);
		installer->force_remove_categories = NULL;
	}
	
	if (installer->problems) {
		log_debug ("-> problem cases");
		eazel_install_problem_handle_cases (installer->problem,
						    installer->service,
						    &(installer->problems),
						    &(installer->install_categories),
						    NULL,
						    NULL);
		eazel_installer_post_install (installer);
		return TRUE;
	} else {
		log_debug ("-> let's go");
		if (installer->successful && installer->install_categories) { 
			log_debug ("   ... ready to install");
			eazel_installer_do_install (installer, installer->install_categories, FALSE);
			eazel_installer_post_install (installer);
			if (installer->problems) {
				log_debug ("   ... had problems");
				return TRUE;
			} 
		}
		
		gnome_druid_set_buttons_sensitive (druid, FALSE, TRUE, TRUE); 
	}
	
	/* FALSE means remove this source */
	return FALSE;
}


void
druid_finish (GnomeDruidPage  *gnomedruidpage,
	      gpointer         arg1,
	      EazelInstaller  *installer)
{
	g_message ("Farewell!");

	if (installer != NULL) {
		if (installer->downloaded_anything) {
			ask_to_delete_rpms (installer);
		}
		gtk_object_unref (GTK_OBJECT (installer));
	}

	g_mem_profile ();
	exit (0);
}

void
prep_install (GnomeDruidPage  *gnomedruidpage,
	      GtkWidget *druid,
	      EazelInstaller  *installer)
{
	g_message ("Prepping install page");
	g_timeout_add (0, (GSourceFunc)begin_install, installer);
}

void
prep_finish (GnomeDruidPage  *gnomedruidpage,
	     GtkWidget *druid,
	     EazelInstaller  *installer)
{
	gnome_druid_set_buttons_sensitive (GNOME_DRUID (druid), FALSE, TRUE, FALSE);
}

void
prep_retry (GnomeDruidPage *gnomedruidpage,
	    GtkWidget *druid,
	    EazelInstaller *installer)
{
	gnome_druid_set_buttons_sensitive (GNOME_DRUID (druid), FALSE, installer->uninstalling ? FALSE : TRUE, TRUE);
}
