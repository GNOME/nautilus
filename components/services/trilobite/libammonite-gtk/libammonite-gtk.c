/* $Id$
 * 
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
 * Author:  Michael Fleming <mfleming@eazel.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "libammonite-gtk.h"
#include <gtk/gtk.h>
#include <gnome.h>

#ifdef DEBUG
#define DEBUG_MSG(x) my_debug_printf x
#include <stdio.h>

static void
my_debug_printf(char *fmt, ...)
{
	va_list args;
	gchar * out;

	g_assert (fmt);

	va_start (args, fmt);

	out = g_strdup_vprintf (fmt, args);

	fprintf (stderr, "DEBUG: %s\n", out);

	g_free (out);
	va_end (args);
}

#else
#define DEBUG_MSG(x) 
#endif

typedef struct {
	gboolean 		done;
	EazelProxy_User *	user;
	EazelProxy_AuthnFailInfo * fail_info;
} LoginSignalState;

typedef struct {
	gpointer 	user_data; 
	AmmonitePromptLoginCb callback;
} PromptLoginState;


static void
authn_cb_succeeded (const EazelProxy_User *user, gpointer state, CORBA_Environment *ev)
{
	PromptLoginState *p_state;

	DEBUG_MSG (("%lu: In authn_cb_succeeded\n", (unsigned long) getpid()));
	
	g_assert (NULL != user);
	g_assert (NULL != state);

	p_state = (PromptLoginState *)state;

	if (p_state->callback) {
		p_state->callback (p_state->user_data, user, NULL);
	}
}

static void
authn_cb_failed (const EazelProxy_User *user, const EazelProxy_AuthnFailInfo *info, gpointer state, CORBA_Environment *ev)
{
	PromptLoginState *p_state;

	DEBUG_MSG (("%lu: In authn_cb_failed\n", (unsigned long) getpid()));
	
	g_assert (NULL != user);
	g_assert (NULL != state);

	p_state = (PromptLoginState *)state;

	if (p_state->callback) {
		p_state->callback (p_state->user_data, user, info);
	}
}


static CORBA_boolean
prompter_cb_prompt_authenticate (
	const EazelProxy_User *user,
	const EazelProxy_AuthnPromptKind kind, 
	EazelProxy_AuthnInfo **authninfo, 
	gpointer user_data, 
	CORBA_Environment *ev
) {
	gchar *password_glib = NULL;
	gchar *username_glib = NULL;
	CORBA_boolean ret;

	ret = FALSE;

	if ( 	EazelProxy_InitialFail == kind
		|| EazelProxy_ReauthnFail == kind
	) {
		ammonite_do_authn_error_dialog();

		*authninfo = EazelProxy_AuthnInfo__alloc();

		(*authninfo)->services_redirect_uri = CORBA_string_dup ("");
		(*authninfo)->services_login_path = CORBA_string_dup ("");
		(*authninfo)->username = CORBA_string_dup ( "" ); 
		(*authninfo)->password = CORBA_string_dup ( "" ); 
	} else {
		/* If there was a username specified, then we're not going to allow
		 * the user to change it
		 */
		if ( '\0' == user->user_name[0] ) {
			ret = ammonite_do_prompt_dialog (NULL, NULL, kind == EazelProxy_Initial, &username_glib, &password_glib);
		} else {
			ret = ammonite_do_prompt_dialog (user->user_name, NULL, kind == EazelProxy_Initial, NULL, &password_glib);
		}

		*authninfo = EazelProxy_AuthnInfo__alloc();

		(*authninfo)->services_redirect_uri = CORBA_string_dup ("");
		(*authninfo)->services_login_path = CORBA_string_dup ("");

		if (ret) {
			if ( '\0' == user->user_name[0] ) {
				(*authninfo)->username = CORBA_string_dup ( (NULL != username_glib) ? username_glib : "" );
			} else {
				(*authninfo)->username = CORBA_string_dup ( user->user_name );
			}
			(*authninfo)->password = CORBA_string_dup ( (NULL != password_glib) ? password_glib : "" ); 

			g_free (username_glib);
			username_glib = NULL;
			g_free (password_glib);
			password_glib = NULL;
		} else {
			(*authninfo)->username = CORBA_string_dup ( "" ); 
			(*authninfo)->password = CORBA_string_dup ( "" ); 
		}	
	}

	return ret;
}

gboolean
ammonite_do_prompt_login_async (
	const char *username, 
	const char *services_redirect_uri, 
	const char *services_login_path,
	gpointer user_data,
	AmmonitePromptLoginCb callback
) {

	CORBA_Environment ev;
	EazelProxy_UserPrompter userprompter = CORBA_OBJECT_NIL;
	EazelProxy_AuthnCallback authn_callback = CORBA_OBJECT_NIL;
	PromptLoginState *p_state;
	EazelProxy_AuthnInfo *authinfo;
	gboolean success;

	AmmoniteAuthCallbackWrapperFuncs authn_cb_funcs = {
		authn_cb_succeeded, authn_cb_failed
	};
	AmmoniteUserPrompterWrapperFuncs up_cb_funcs = {
		prompter_cb_prompt_authenticate
	};

	CORBA_exception_init (&ev);

	p_state = g_new0 (PromptLoginState, 1);
	p_state->user_data = user_data;
	p_state->callback = callback;

	userprompter = ammonite_userprompter_wrapper_new (ammonite_get_poa(), &up_cb_funcs, p_state);
	authn_callback = ammonite_auth_callback_wrapper_new (ammonite_get_poa(), &authn_cb_funcs, p_state);

	if (CORBA_OBJECT_NIL == authn_callback) {
		g_warning ("Couldn't create AuthnCallback");
		success = FALSE;
		goto error;
	}

	if (CORBA_OBJECT_NIL == userprompter) {
		g_warning ("Couldn't create UserPrompter\n");
		success = FALSE;
		goto error;
	}

	authinfo = EazelProxy_AuthnInfo__alloc ();
	authinfo->username = CORBA_string_dup (NULL == username ? "" : username);
	authinfo->password = CORBA_string_dup ("");
	authinfo->services_redirect_uri = CORBA_string_dup (services_redirect_uri ? services_redirect_uri : "");
	authinfo->services_login_path = CORBA_string_dup (services_login_path ? services_login_path : "");

	DEBUG_MSG (("%lu: Calling prompt_authenticate\n", (unsigned long) getpid()));

	EazelProxy_UserControl_prompt_authenticate_user (ammonite_get_user_control(), authinfo, TRUE, userprompter, authn_callback, &ev);

	DEBUG_MSG (("%lu: Back from prompt_authenticate\n", (unsigned long) getpid()));

	if (CORBA_NO_EXCEPTION != ev._major) {
		g_warning ("Exception during prompt_authenticate_user");
		success = FALSE;
		goto error;
	}

	success = TRUE;

error:
	if (! success ) {
		ammonite_auth_callback_wrapper_free (ammonite_get_poa(), authn_callback);
		ammonite_userprompter_wrapper_free (ammonite_get_poa(), userprompter);
		g_free (p_state);
	}
	
	CORBA_exception_free (&ev);

	return success;
}

static void /* AmmonitePromptLoginCb */
prompt_login_sync_cb (gpointer user_data, const EazelProxy_User *user, const EazelProxy_AuthnFailInfo *fail_info)
{
	LoginSignalState *p_state;

	p_state = (LoginSignalState *)user_data;

	p_state->done = TRUE;
	p_state->user = EazelProxy_User_duplicate (user);
	p_state->fail_info = EazelProxy_AuthnFailInfo_duplicate (fail_info);
}

EazelProxy_User *
ammonite_do_prompt_login (
	const char *username, 
	const char *services_redirect_uri, 
	const char *services_login_path,
	/*OUT*/ CORBA_long *p_fail_code
) {

	volatile LoginSignalState state;
	EazelProxy_User *user = NULL;
 	CORBA_long fail_code = 0;
	gboolean success;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	memset ((void *)&state, 0, sizeof(state) );

	success = ammonite_do_prompt_login_async (
		username, 
		services_redirect_uri, 
		services_login_path,
		(gpointer)&state,
		prompt_login_sync_cb
	);

	if (success) {
		EazelProxy_UserControl user_control;

		user_control = ammonite_get_user_control();
		while ( ! state.done && ! CORBA_Object_non_existent (user_control, &ev) ) {
			g_main_iteration(TRUE);
		}

		if (CORBA_Object_non_existent (user_control, &ev)) {
			DEBUG_MSG (("%lu: CORBA server disappeared\n",(unsigned long) getpid()));
			goto error;
		}

		DEBUG_MSG (("%lu: Response iteration complete, success=%s\n", (unsigned long) getpid(), state.fail_info ? "FALSE" : "TRUE"));

		if (state.fail_info && p_fail_code) {
			*p_fail_code = state.fail_info->code;
			fail_code = state.fail_info->code;
			CORBA_free (state.fail_info);
			CORBA_free (state.user);
		} else {
			user = state.user;
			/* and fail_info is NULL */
		}
	}

	if ( ! user
		  && ( EAZELPROXY_AUTHN_FAIL_NETWORK == fail_code
		  		|| EAZELPROXY_AUTHN_FAIL_SERVER == fail_code
		  )
	) {
		ammonite_do_network_error_dialog();
	}

error:
	CORBA_exception_free (&ev);

	return user;
}

gboolean
ammonite_do_prompt_dialog (
	const char *user, 
	const char *pw, 
	gboolean retry, 
	char **p_user, 
	char **p_pw
) {

	GtkWidget *dialog;
	gboolean dialog_return;

	g_return_val_if_fail ( NULL != user || NULL != p_user, FALSE);
	g_return_val_if_fail ( NULL != p_pw, FALSE);

	if (p_user) {
		*p_user = NULL;
	}
	*p_pw = NULL;

	DEBUG_MSG (("Opening Dialog\n"));

	dialog = ammonite_login_dialog_new ( 
			"Eazel Services Login", 
			retry 	? "Please log into Eazel Services"
				: "You username or password was incorrect.  Please try again.", 
			"", 
			"", 
			FALSE,
			FALSE
		 );

	if ( NULL != user && '\0' != user[0] ) {
		ammonite_login_dialog_set_username ( 
			AMMONITE_LOGIN_DIALOG (dialog),
			user
		);
		ammonite_login_dialog_set_readonly_username ( 
			AMMONITE_LOGIN_DIALOG (dialog),
			TRUE
		);
	}

	dialog_return = ammonite_login_dialog_run_and_block (AMMONITE_LOGIN_DIALOG (dialog));

	if (dialog_return) {
		if ( NULL != p_user ) {
			*p_user = ammonite_login_dialog_get_username (AMMONITE_LOGIN_DIALOG (dialog));
		}
		if ( NULL != p_pw ) {
			*p_pw = ammonite_login_dialog_get_password (AMMONITE_LOGIN_DIALOG (dialog));		
		}
	} else {
		DEBUG_MSG (("User cancelled...\n"));
	}

	gtk_widget_destroy (dialog);

#if 0
	/* Clean up events after the dialog */
	while (gtk_events_pending()) {
		gtk_main_iteration();
	}
#endif
	return dialog_return;
}

void
ammonite_do_authn_error_dialog ()
{
	GtkWidget *dialog;

	DEBUG_MSG (("Opening Dialog\n"));

	dialog = gnome_error_dialog ("I'm sorry, your Eazel Service username or password are incorrect.");

	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));

	/* Clean up events after the dialog */
	while (gtk_events_pending()) {
		gtk_main_iteration();
	}
}

void
ammonite_do_network_error_dialog ()
{
	GtkWidget *dialog;

	DEBUG_MSG (("Opening Network Error Dialog\n"));

	dialog = gnome_error_dialog ("I'm sorry, network problems are preventing you from connecting to Eazel Services.");

	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));

	/* Clean up events after the dialog */
	while (gtk_events_pending()) {
		gtk_main_iteration();
	}
}


