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

#include <bonobo/bonobo-control.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-bonobo-extensions.h>
#include <libnautilus-extensions/nautilus-caption-table.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-font-factory.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include <libnautilus-extensions/nautilus-string.h>
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
#include "nautilus-summary-footer.h"
#include "nautilus-summary-view-private.h"

#define notDEBUG_TEST	1
#define notDEBUG_PEPPER	1


void
footer_item_clicked_callback (GtkWidget *widget, int index, gpointer callback_data)
{
	NautilusSummaryView *view;

	g_return_if_fail (NAUTILUS_IS_SUMMARY_VIEW (callback_data));
	g_return_if_fail (index >= FOOTER_REGISTER_OR_PREFERENCES);
	g_return_if_fail (index <= FOOTER_PRIVACY_STATEMENT);

	view = NAUTILUS_SUMMARY_VIEW (callback_data);

	switch (index) {
	case FOOTER_REGISTER_OR_PREFERENCES:
		if (!view->details->logged_in) {
			register_button_cb (NULL, view);
		} else {
			preferences_button_cb (NULL, view);
		}
		break;

	case FOOTER_LOGIN_OR_LOGOUT:
		if (!view->details->logged_in) {
			generate_login_dialog (view);
		} else {
			logout_button_cb (NULL, view);
		}
		break;

	case FOOTER_TERMS_OF_USER:
		nautilus_view_open_location_in_this_window (view->details->nautilus_view, SUMMARY_TERMS_OF_USE_URI);
		break;

	case FOOTER_PRIVACY_STATEMENT:
		nautilus_view_open_location_in_this_window (view->details->nautilus_view, SUMMARY_PRIVACY_STATEMENT_URI);
		break;

	default:
		g_assert_not_reached ();
		break;
	}
}
