/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-window-toolbars.c - implementation of nautilus window toolbar operations,
                                split into separate file just for convenience.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: John Sullivan <sullivan@eazel.com>
*/

#include "ntl-window-private.h"
#include "ntl-app.h"

#include <gnome.h>

/* forward declarations */
static void nautilus_window_reload_cb (GtkWidget *widget, NautilusWindow *window);
static void nautilus_window_stop_cb (GtkWidget *widget, NautilusWindow *window);


/* toolbar definitions */

#define TOOLBAR_BACK_BUTTON_INDEX	0
#define TOOLBAR_FORWARD_BUTTON_INDEX	1
#define TOOLBAR_UP_BUTTON_INDEX		2
#define TOOLBAR_RELOAD_BUTTON_INDEX	3
/* separator */
#define TOOLBAR_HOME_BUTTON_INDEX	5
/* separator */
#define TOOLBAR_STOP_BUTTON_INDEX	7

static GnomeUIInfo toolbar_info[] = {
	GNOMEUIINFO_ITEM_STOCK
	(N_("Back"), N_("Go to the previously visited directory"),
	 nautilus_window_back_cb, GNOME_STOCK_PIXMAP_BACK),
	GNOMEUIINFO_ITEM_STOCK
	(N_("Forward"), N_("Go to the next directory"),
	 nautilus_window_forward_cb, GNOME_STOCK_PIXMAP_FORWARD),
	GNOMEUIINFO_ITEM_STOCK
	(N_("Up"), N_("Go up a level in the directory heirarchy"),
	 nautilus_window_up_cb, GNOME_STOCK_PIXMAP_UP),
	GNOMEUIINFO_ITEM_STOCK
	(N_("Reload"), N_("Reload this view"),
	 nautilus_window_reload_cb, GNOME_STOCK_PIXMAP_REFRESH),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_STOCK
	(N_("Home"), N_("Go to your home directory"),
	 nautilus_window_home_cb, GNOME_STOCK_PIXMAP_HOME),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_STOCK
	(N_("Stop"), N_("Interrupt loading"),
	 nautilus_window_stop_cb, GNOME_STOCK_PIXMAP_STOP),
	GNOMEUIINFO_END
};

void
nautilus_window_initialize_toolbars (NautilusWindow *window)
{
	GnomeApp *app;
	GtkWidget *toolbar;

	app = GNOME_APP (window);

	toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);
	gnome_app_fill_toolbar_with_data (GTK_TOOLBAR (toolbar), toolbar_info, app->accel_group, app);
	gnome_app_set_toolbar (app, GTK_TOOLBAR (toolbar));

	bonobo_ui_handler_set_toolbar (window->uih, "Main", toolbar);

	/* Remember some widgets now so their state can be changed later */
	window->back_button = toolbar_info[TOOLBAR_BACK_BUTTON_INDEX].widget;
	window->forward_button = toolbar_info[TOOLBAR_FORWARD_BUTTON_INDEX].widget;
	window->up_button = toolbar_info[TOOLBAR_UP_BUTTON_INDEX].widget;
	window->reload_button = toolbar_info[TOOLBAR_RELOAD_BUTTON_INDEX].widget;
	window->stop_button = toolbar_info[TOOLBAR_STOP_BUTTON_INDEX].widget;
}

static void
nautilus_window_reload_cb (GtkWidget *widget, NautilusWindow *window)
{
	Nautilus_NavigationRequestInfo nri;

	memset(&nri, 0, sizeof(nri));
	nri.requested_uri = (char *)nautilus_window_get_requested_uri (window);
	nri.new_window_default = nri.new_window_suggested = nri.new_window_enforced = Nautilus_V_FALSE;
	nautilus_window_change_location (window, &nri, NULL, FALSE, TRUE);
}

static void
nautilus_window_stop_cb (GtkWidget *widget, NautilusWindow *window)
{
	nautilus_window_set_state_info (window, RESET_TO_IDLE, 0);
}

