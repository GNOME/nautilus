/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-service-menu.c: integrate the built-in service menu
 
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

#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-ui-util.h>

#include "nautilus-window.h"
#include "nautilus-window-private.h"


static void
goto_services_summary (BonoboUIComponent *component, 
			       gpointer user_data, 
			       const char *verb)
{
	nautilus_window_goto_uri (NAUTILUS_WINDOW (user_data), "eazel:");
}

static void
goto_online_storage (BonoboUIComponent *component, 
			       gpointer user_data, 
			       const char *verb)
{
	nautilus_window_goto_uri (NAUTILUS_WINDOW (user_data), "eazel:vault");
}

static void
goto_software_catalog (BonoboUIComponent *component, 
			       gpointer user_data, 
			       const char *verb)
{
	nautilus_window_goto_uri (NAUTILUS_WINDOW (user_data), "eazel:catalog");
}

void	nautilus_window_install_service_menu (NautilusWindow *window)
{
	BonoboUIComponent *ui_component;
	
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("Eazel Services", goto_services_summary),
		BONOBO_UI_VERB ("Online Storage", goto_online_storage),
		BONOBO_UI_VERB ("Software Catalog", goto_software_catalog),
		BONOBO_UI_VERB_END
	};

	ui_component = bonobo_ui_component_new ("Eazel Services");
	
	bonobo_ui_component_set_container
		(ui_component,
		 bonobo_object_corba_objref (BONOBO_OBJECT (window->details->ui_container)));
	bonobo_ui_component_freeze (ui_component, NULL);

	bonobo_ui_util_set_ui (ui_component,
			       DATADIR,
			       "nautilus-service-ui.xml",
			       "nautilus");
	bonobo_ui_component_thaw (ui_component, NULL);

	bonobo_ui_component_add_verb_list_with_data (ui_component, verbs, window);
}


