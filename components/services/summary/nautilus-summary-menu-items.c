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
 * Author: J Shane Culpepper <pepper@eazel.com>
 */

#include <config.h>

#include <gnome-xml/tree.h>
#include <bonobo/bonobo-control.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include <eel/eel-background.h>
#include <libnautilus-extensions/nautilus-bonobo-extensions.h>
#include <eel/eel-caption-table.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-font-factory.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <libnautilus-extensions/nautilus-tabs.h>

#include <libgnomeui/gnome-stock.h>
#include <stdio.h>
#include <unistd.h>

#include <orb/orbit.h>
#include <liboaf/liboaf.h>
#include <libtrilobite/trilobite-redirect.h>
#include <libtrilobite/eazelproxy.h>
#include <libtrilobite/libammonite.h>
#include <bonobo/bonobo-main.h>

#include "nautilus-summary-view.h"
#include "eazel-summary-shared.h"

#include "eazel-services-footer.h"
#include "eazel-services-header.h"
#include "eazel-services-extensions.h"

#include "nautilus-summary-callbacks.h"
#include "nautilus-summary-dialogs.h"
#include "nautilus-summary-menu-items.h"
#include "nautilus-summary-view-private.h"

#define notDEBUG_PEPPER	1

static void	bonobo_register_callback	(BonoboUIComponent		*ui,
						 gpointer			user_data,
						 const char			*verb);
static void	bonobo_login_callback		(BonoboUIComponent		*ui,
						 gpointer			user_data,
						 const char			*verb);
static void	bonobo_logout_callback		(BonoboUIComponent		*ui,
						 gpointer			user_data,
						 const char			*verb);
static void	bonobo_preferences_callback	(BonoboUIComponent		*ui,
						 gpointer			user_data,
						 const char			*verb);


/* update the visibility of the menu items according to the login state */
void
update_menu_items (NautilusSummaryView *view, gboolean logged_in)
{
	BonoboUIComponent *ui;

	ui = bonobo_control_get_ui_component 
		(nautilus_view_get_bonobo_control 
			(view->details->nautilus_view)); 

	nautilus_bonobo_set_hidden (ui,
				    "/commands/Register",
				    logged_in);
	
	nautilus_bonobo_set_hidden (ui,
				     "/commands/Login",
				    logged_in);

	nautilus_bonobo_set_hidden (ui,
				    "/commands/Preferences",
				    !logged_in);
	
	nautilus_bonobo_set_hidden (ui,
				    "/commands/Logout",
				    !logged_in);				    				    
}

/* this routine is invoked when the view is activated to merge in our menu items */
void
merge_bonobo_menu_items (BonoboControl *control, gboolean state, gpointer user_data)
{
 	NautilusSummaryView *view;
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("Register", bonobo_register_callback),
		BONOBO_UI_VERB ("Login", bonobo_login_callback),
		BONOBO_UI_VERB ("Logout", bonobo_logout_callback),
		BONOBO_UI_VERB ("Preferences", bonobo_preferences_callback),		
		BONOBO_UI_VERB_END
	};

	g_assert (BONOBO_IS_CONTROL (control));
	
	view = NAUTILUS_SUMMARY_VIEW (user_data);

	if (state) {
		gboolean logged_in;
		char * user_name;
	
		nautilus_view_set_up_ui (view->details->nautilus_view,
				         DATADIR,
					 "nautilus-summary-view-ui.xml",
					 "nautilus-summary-view");
									
		bonobo_ui_component_add_verb_list_with_data 
			(bonobo_control_get_ui_component (control), verbs, view);

		user_name = ammonite_get_default_user_username ();
		logged_in = (NULL != user_name);
		update_menu_items (view, logged_in);
		g_free (user_name);
	}

        /* Note that we do nothing if state is FALSE. Nautilus content
         * views are never explicitly deactivated
	 */
}

/* here are the callbacks to handle bonobo menu items */
static void
bonobo_register_callback (BonoboUIComponent *ui, gpointer user_data, const char *verb)
{
	NautilusSummaryView *view;
	
	view = NAUTILUS_SUMMARY_VIEW (user_data);
	register_button_cb (NULL, view);
}

static void
bonobo_login_callback (BonoboUIComponent *ui, gpointer user_data, const char *verb)
{
	NautilusSummaryView *view;
	
	view = NAUTILUS_SUMMARY_VIEW (user_data);
	nautilus_summary_show_login_dialog (view);
}

static void
bonobo_logout_callback (BonoboUIComponent *ui, gpointer user_data, const char *verb)
{
	NautilusSummaryView *view;
	
	view = NAUTILUS_SUMMARY_VIEW (user_data);
	logout_button_cb (NULL, view);
}

static void
bonobo_preferences_callback (BonoboUIComponent *ui, gpointer user_data, const char *verb)
{
	NautilusSummaryView *view;
	
	view = NAUTILUS_SUMMARY_VIEW (user_data);
	preferences_button_cb (NULL, view);
}

