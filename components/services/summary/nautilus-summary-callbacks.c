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

#include <libgnomeui/gnome-stock.h>
#include <stdio.h>
#include <unistd.h>

#include <orb/orbit.h>
#include <liboaf/liboaf.h>
#include <bonobo/bonobo-main.h>

#include <libnautilus-extensions/nautilus-caption-table.h>
#include <libtrilobite/eazelproxy.h>
#include <libtrilobite/libammonite.h>
#include <libtrilobite/trilobite-redirect.h>

#include "nautilus-summary-view.h"
#include "eazel-summary-shared.h"
#include "nautilus-summary-dialogs.h"
#include "nautilus-summary-menu-items.h"
#include "nautilus-summary-callbacks.h"
#include "nautilus-summary-view-private.h"

#define notDEBUG_PEPPER	1

static void	authn_cb_succeeded			(const EazelProxy_User		*user,
	       						 gpointer			state,
							 CORBA_Environment		*ev);
static void	authn_cb_failed				(const EazelProxy_User		*user,
							 const EazelProxy_AuthnFailInfo *info,
							 gpointer			state,
							 CORBA_Environment		*ev);


/* Be careful not to invoke another HTTP request; this call is
 * invoked from a two-way CORBA call from ammonite
 */
static void
authn_cb_succeeded (const EazelProxy_User *user, gpointer state, CORBA_Environment *ev)
{
	NautilusSummaryView    *view;
	gint			timeout;

	view = NAUTILUS_SUMMARY_VIEW (state);

	g_assert (Pending_Login == view->details->pending_operation);

	view->details->pending_operation = Pending_None;
	
	timeout = gtk_timeout_add (0, logged_in_callback, view);

	bonobo_object_unref (BONOBO_OBJECT (view->details->nautilus_view));
}

/* Be careful not to invoke another HTTP request; this call is
 * invoked from a two-way CORBA call from ammonite
 */
static void
authn_cb_failed (const EazelProxy_User *user, const EazelProxy_AuthnFailInfo *info, gpointer state, CORBA_Environment *ev)
{
	NautilusSummaryView    *view;

	view = NAUTILUS_SUMMARY_VIEW (state);

	g_assert (Pending_Login == view->details->pending_operation);

	view->details->pending_operation = Pending_None;

	view->details->logged_in = FALSE;
	
	update_menu_items (view, FALSE);

	if (info && ( info->code == EAZELPROXY_AUTHN_FAIL_NETWORK
	    || info->code == EAZELPROXY_AUTHN_FAIL_SERVER)) {
		nautilus_summary_show_login_failure_dialog (view, _("Sorry, network problems are preventing you from connecting to Eazel Services."));
		view->details->attempt_number = 0;
		view->details->current_attempt = initial;
	} else if (info && ( info->code == EAZELPROXY_AUTHN_FAIL_USER_NOT_ACTIVATED)) {
		/* FIXME we really should use the services alert icon here, eh? */
		nautilus_summary_show_login_failure_dialog (view, _("Your Eazel Services account has not yet been activated.  "
			    "You can't log into Eazel Services until you activate your account.\n\n"
			    "Please check your email for activation instructions."));
		view->details->attempt_number = 0;
		view->details->current_attempt = initial;
	} else if (info && ( info->code == EAZELPROXY_AUTHN_FAIL_USER_DISABLED)) {
		/* FIXME we really should use the services alert icon here, eh? */
		nautilus_summary_show_login_failure_dialog (view, _("Your Eazel Service User Account has been temporarily disabled.\n\n"
			    "Please try again in a few minutes, or contact Eazel support if this problem continues."));
		view->details->attempt_number = 0;
		view->details->current_attempt = initial;
	} else {
		/* Most likely error: bad username or password */

		view->details->attempt_number++;

		/* FIXME it would be best to display an error dialog
		 * explaining the problem and offering at least an "I forgot
		 * my password" button (and possibly a "Register" button as well)
		 * In any vase, the dialog that's here is insufficient
		 */

#if 0
		if (view->details->attempt_number > 0 && view->details->attempt_number < 5) {
#endif
			view->details->current_attempt = retry;
			nautilus_summary_show_login_dialog (view);
#if 0
		} else {
			nautilus_summary_login_failure_dialog (view, _("We're sorry, but your name and password are still not recognized."));
			view->details->attempt_number = 0;
			view->details->current_attempt = initial;
		}
#endif
	}
	
	bonobo_object_unref (BONOBO_OBJECT (view->details->nautilus_view));
}

/* callback to handle the login button.  Right now only does a simple redirect. */
void
login_button_cb (GtkWidget      *button, NautilusSummaryView    *view)
{
	char		*user_name;
	char		*password;
	EazelProxy_AuthnInfo *authinfo;
	CORBA_Environment ev;

	AmmoniteAuthCallbackWrapperFuncs cb_funcs = {
		authn_cb_succeeded, authn_cb_failed
	};

	CORBA_exception_init (&ev);

	g_assert (Pending_None == view->details->pending_operation);

	/* FIXME this doesn't actually handle the case when user_control is NIL
	 * very well.  No callback is generated, so no user feedback is generated
	 * and the summary view is left in an illegal state
	 */

	if (CORBA_OBJECT_NIL != view->details->user_control) {
		view->details->authn_callback = ammonite_auth_callback_wrapper_new (bonobo_poa(), &cb_funcs, view);

		user_name = nautilus_caption_table_get_entry_text (NAUTILUS_CAPTION_TABLE (view->details->caption_table), 0);
		password = nautilus_caption_table_get_entry_text (NAUTILUS_CAPTION_TABLE (view->details->caption_table), 1);

		authinfo = EazelProxy_AuthnInfo__alloc ();
		authinfo->username = CORBA_string_dup (user_name);
		authinfo->password = CORBA_string_dup (password);
		user_name = NULL;
		password = NULL;
				
		authinfo->services_redirect_uri = CORBA_string_dup ("");
		authinfo->services_login_path = CORBA_string_dup ("");

		/* Ref myself until the callback returns */
		bonobo_object_ref (BONOBO_OBJECT (view->details->nautilus_view));

		view->details->pending_operation = Pending_Login;
		
		EazelProxy_UserControl_authenticate_user (
			view->details->user_control,
			authinfo, TRUE, 
			view->details->authn_callback, &ev
		);

		if (CORBA_NO_EXCEPTION != ev._major) {
			g_warning ("Exception during EazelProxy login");
			/* FIXME bugzilla.eazel.com 2745: cleanup after fail here */
		}


	}
	
	CORBA_exception_free (&ev);
}

/* callback to handle the logout button.  Right now only does a simple redirect. */
void
logout_button_cb (GtkWidget      *button, NautilusSummaryView    *view)
{
	CORBA_Environment	ev;
	EazelProxy_UserList	*users;
	CORBA_unsigned_long	i;
	gint			timeout;
	CORBA_exception_init (&ev);

	if (CORBA_OBJECT_NIL != view->details->user_control) {
	/* Get list of currently active users */

		users = EazelProxy_UserControl_get_active_users (
			view->details->user_control, &ev
		);

		if (CORBA_NO_EXCEPTION != ev._major) {
			g_message ("Exception while logging out user");
			return;
		}

	/* Log out the current default user */
		for (i = 0; i < users->_length ; i++) {
			EazelProxy_User *cur;

			cur = users->_buffer + i;

			if (cur->is_default) {
				g_message ("Logging out user '%s'", cur->user_name);
				EazelProxy_UserControl_logout_user (
					view->details->user_control,
					cur->proxy_port, &ev
				);
				break;
			}
		}

		CORBA_free (users);
	}

	timeout = gtk_timeout_add (0, logged_out_callback, view);

	CORBA_exception_free (&ev);
}

gint
logged_in_callback (gpointer	raw)
{
	NautilusSummaryView	*view;

	view = NAUTILUS_SUMMARY_VIEW (raw);
	view->details->logged_in = TRUE;

	update_menu_items (view, TRUE);
	nautilus_view_open_location_in_this_window
		(view->details->nautilus_view, "eazel:");

	return (FALSE);
}


gint
logged_out_callback (gpointer	raw)
{
	NautilusSummaryView	*view;

	view = NAUTILUS_SUMMARY_VIEW (raw);
	view->details->logged_in = FALSE;
	
	update_menu_items (view, FALSE);
	nautilus_view_open_location_in_this_window
		(view->details->nautilus_view, "eazel:");

	return (FALSE);
}

/* callback to handle the maintenance button.  Right now only does a simple redirect. */
void
preferences_button_cb (GtkWidget      *button, NautilusSummaryView    *view)
{
	char	*url;
	url = NULL;

	url = trilobite_redirect_lookup (PREFERENCES_KEY);
	if (!url) {
		g_error ("Failed to load Registration url!");
	}

	nautilus_view_open_location_in_this_window
		(view->details->nautilus_view, url);
	g_free (url);

}

/* callback to handle the forgotten password button. */
void
forgot_password_button_cb (GtkWidget      *button, NautilusSummaryView    *view)
{

	nautilus_view_open_location_in_this_window
		(view->details->nautilus_view, SUMMARY_CHANGE_PWD_FORM);

}

/* callback to handle the register button.  Right now only does a simple redirect. */
void
register_button_cb (GtkWidget      *button, NautilusSummaryView    *view)
{
	char	*url;
	url = NULL;

	url = trilobite_redirect_lookup (REGISTER_KEY);
	if (!url) {
		g_error ("Failed to load Registration url!");
	}

	nautilus_view_open_location_in_this_window
		(view->details->nautilus_view, url);
	g_free (url);

}
