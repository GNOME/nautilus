/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-service-ui.c: integrate the built-in service ui
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Andy Hertzfeld <andy@eazel.com>
*/

#include <config.h>
#include "nautilus-window-service-ui.h"

#include <bonobo/bonobo-ui-util.h>
#include "nautilus-window-private.h"
#include <gtk/gtksignal.h>
#ifdef EAZEL_SERVICES
#include <libtrilobite/libammonite.h>
#include <bonobo/bonobo-main.h>

static void
goto_services_summary (BonoboUIComponent *component, 
		       gpointer callback_data, 
		       const char *verb)
{
	nautilus_window_go_to (NAUTILUS_WINDOW (callback_data),
			       "eazel:");
}

static void
goto_online_storage (BonoboUIComponent *component, 
		     gpointer callback_data, 
		     const char *verb)
{
	char			*url;
	char			*user_name;

	if ( ammonite_init (bonobo_poa())) {
		user_name = ammonite_get_default_user_username ();
	} else {
		user_name = NULL;
	}

	if (user_name == NULL) {
		url = g_strdup ("eazel:");
		/* FIXME bugzilla.eazel.com 5036: user feedback needs to be displayed in this case */
		/* Something better than just going to the summary page */

	} else {
		url = g_strdup_printf ("eazel-services:///~%s", user_name);
		g_free (user_name);
		user_name = NULL;
	}
	nautilus_window_go_to (NAUTILUS_WINDOW (callback_data), url);
	g_free (url);
	url = NULL;
}

static void
goto_software_catalog (BonoboUIComponent *component, 
		       gpointer callback_data, 
		       const char *verb)
{
	char			*url;
	gboolean		logged_in;
	char 			*user_name;

	if (ammonite_init (bonobo_poa())) {
		user_name = ammonite_get_default_user_username ();

		logged_in = (NULL != user_name);
		g_free (user_name);
	} else {
		logged_in = FALSE;
	}


	if (!logged_in) {
		url = g_strdup ("eazel-services://anonymous/catalog");
	} else {
		url = g_strdup ("eazel-services:///catalog");
	}

	nautilus_window_go_to (NAUTILUS_WINDOW (callback_data), url);
	g_free (url);
	url = NULL;

}

static void
goto_services_support (BonoboUIComponent *component, 
		       gpointer callback_data, 
		       const char *verb)
{
	nautilus_window_go_to (NAUTILUS_WINDOW (callback_data),
			       "http://services.eazel.com/support");

}

static void
detach_service_ui (GtkObject *object,
		   gpointer callback_data)
{
	BonoboUIComponent *service_ui;

	service_ui = BONOBO_UI_COMPONENT (callback_data);
	bonobo_ui_component_unset_container (service_ui);
	bonobo_object_unref (BONOBO_OBJECT (service_ui));
}

void
nautilus_window_install_service_ui (NautilusWindow *window)
{
	BonoboUIComponent *service_ui;
	
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("Eazel Services", goto_services_summary),
		BONOBO_UI_VERB ("Online Storage", goto_online_storage),
		BONOBO_UI_VERB ("Software Catalog", goto_software_catalog),
		BONOBO_UI_VERB ("Services Support", goto_services_support),
		BONOBO_UI_VERB_END
	};

	/* Load UI from the XML file. */
	service_ui = bonobo_ui_component_new ("Eazel Services");
	bonobo_ui_component_add_verb_list_with_data (service_ui, verbs, window);
	bonobo_ui_component_set_container
		(service_ui,
		 nautilus_window_get_ui_container (window));
	bonobo_ui_component_freeze (service_ui, NULL);
	bonobo_ui_util_set_ui (service_ui,
			       DATADIR,
			       "nautilus-service-ui.xml",
			       "nautilus");
	bonobo_ui_component_thaw (service_ui, NULL);

	/* Get rid of the UI when the window goes away. */
	gtk_signal_connect (GTK_OBJECT (window),
			    "destroy",
			    detach_service_ui,
			    service_ui);
}

#endif

