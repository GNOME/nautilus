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
	exit (1);
}


void
druid_delete (GtkWidget *widget, GdkEvent *event, EazelInstaller *installer)
{
	exit (1);
}


gboolean
begin_install (EazelInstaller  *installer)
{
	GtkWidget *window = installer->window;
	GnomeDruid *druid;
	GList *iterator;
	GList *install_categories = NULL;

	druid = GNOME_DRUID (gtk_object_get_data (GTK_OBJECT (window), "druid"));

	gnome_druid_set_buttons_sensitive(druid,FALSE,FALSE,TRUE);

	for (iterator = installer->categories; iterator; iterator = iterator->next) {
		CategoryData *category = (CategoryData*)iterator->data;
		GtkWidget *widget = GTK_WIDGET (gtk_object_get_data (GTK_OBJECT (window), 
								     category->name));
		
		if (GTK_TOGGLE_BUTTON (widget)->active) {
			install_categories = g_list_append (install_categories, category);
		}
	}

	if (install_categories) {
		if (eazel_installer_do_install (installer, install_categories)) {
			/* still more to do... */
			return FALSE;
		}
	}

	gnome_druid_set_buttons_sensitive(druid, FALSE, TRUE, TRUE);
	
	return FALSE;
}


void
druid_finish (GnomeDruidPage  *gnomedruidpage,
	      gpointer         arg1,
	      EazelInstaller  *installer)
{
	exit (0);
}

void
prep_install (GnomeDruidPage  *gnomedruidpage,
	      GtkWidget *druid,
	      EazelInstaller  *installer)
{
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
prep_lock (GnomeDruidPage *gnomedruidpage,
	   GtkWidget *druid,
	   EazelInstaller *installer)
{
	gnome_druid_set_buttons_sensitive (GNOME_DRUID (druid), FALSE, FALSE, TRUE);
}
