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


void
druid_cancel (GnomeDruid      *gnomedruid,
	      EazelInstaller  *installer)
{
	g_mem_profile ();
	exit (1);
}


void
druid_delete (GtkWidget *widget, GdkEvent *event, EazelInstaller *installer)
{
	g_mem_profile ();
	exit (1);
}


gboolean
begin_install (EazelInstaller  *installer)
{
	GtkWidget *window = installer->window;
	GnomeDruid *druid;

	g_message ("I'm now here : %s:%d", __FILE__, __LINE__);
	druid = GNOME_DRUID (gtk_object_get_data (GTK_OBJECT (window), "druid"));

	gnome_druid_set_buttons_sensitive(druid,FALSE,FALSE,TRUE);

	/* First time ? If so, check which categories were marked */
	if (installer->install_categories == NULL) {
		GList *iterator;
		GList *install_categories = NULL;
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

	if (installer->force_remove_categories) {
		g_message ("using force_remove_categories");
		eazel_installer_do_install (installer, installer->force_remove_categories, TRUE, TRUE);
		/* still more to do... */
		categorydata_list_destroy (installer->force_remove_categories);
		installer->force_remove_categories = NULL;
		g_message ("I'm now here : %s:%d", __FILE__, __LINE__);
		/* return TRUE; */
	} 
	if (installer->failure_info==NULL && installer->force_categories) {
		g_message ("using force_categories");
		eazel_installer_do_install (installer, installer->force_categories, TRUE, FALSE);
		categorydata_list_destroy (installer->force_categories);
		installer->force_categories = NULL;
		g_message ("I'm now here : %s:%d", __FILE__, __LINE__);
		/* return TRUE; */
	} 

	if (installer->failure_info==NULL && installer->install_categories) { 
		g_message ("using install_categories");
		eazel_installer_do_install (installer, installer->install_categories, FALSE, FALSE);
	}

	g_message ("I'm now here : %s:%d", __FILE__, __LINE__);
	gnome_druid_set_buttons_sensitive(druid, FALSE, TRUE, TRUE); 
	
	/* FALSE means remove this source */
	return FALSE;
}


void
druid_finish (GnomeDruidPage  *gnomedruidpage,
	      gpointer         arg1,
	      EazelInstaller  *installer)
{
	char *package_list;

	/* for now, always delete the rpm files on exit */
	g_message ("Farewell -- deleting RPM files.");
	package_list = g_strdup_printf ("%s/package-list.xml", installer->tmpdir);
	unlink (package_list);
	g_free (package_list);
	eazel_install_delete_downloads (installer->service);

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
	gnome_druid_set_buttons_sensitive (GNOME_DRUID (druid), FALSE, TRUE, TRUE);
}
