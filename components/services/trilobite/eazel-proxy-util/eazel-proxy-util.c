/* $Id$
 * 
 * eazel-proxy-util:
 * 
 * A client an tool for "eazel-proxy", aka ammonite.
 * Allows logging in, logging out, querying of status, etc
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

#include <libtrilobite/libammonite.h>
#include <libammonite-gtk.h>

#include <stdio.h>
#include <orb/orbit.h>
#include <liboaf/liboaf.h>
#include <popt.h>
#include <unistd.h>
#include <signal.h>
#include <glib.h>
#include <stdlib.h>
#include <time.h>
#include <gnome.h>
#include <libgnomeui/libgnomeui.h> 

/* If authentication fails--for whatever reason--ignore subsequent requests for a few seconds
 * so we don't get redundent login boxes from the same operation
 */
#define IGNORE_LOGIN_AFTER_FAIL_SECONDS 2

#define MKF_DEBUG
#ifdef MKF_DEBUG
#define DEBUG_MSG(x) fprintf x;
#else
#define DEBUG_MSG(x)
#endif

EazelProxy_UserControl gl_user_control;
PortableServer_POA gl_poa;

char * opt_login	= NULL;
int opt_login_prompt	= 0;
int opt_status		= 0;
int opt_follow		= 0;
char * opt_password	= NULL;
char * opt_logout	= NULL;
int opt_translate	= 0;
int opt_help		= 0;
char * opt_target_path  = NULL;
char * opt_login_path	= NULL;
#if 0
int opt_dialog		= 0;
#endif
int opt_chpw		= 0;
#ifdef ENABLE_REAUTHN
int opt_reauthn_listen	= 0;
#endif /* ENABLE_REAUTHN */

static const struct poptOption popt_options[] = {
#if 0
	{"dialog", 'd', POPT_ARG_NONE, &opt_dialog, 0, "Prompt for login w/ dialog box", NULL},
#endif
	{"login", 'l', POPT_ARG_STRING, &opt_login, 0, "Log in eazel service user", "<username>"},
	{"login-prompt", 'P', POPT_ARG_NONE, &opt_login_prompt, 0, "Log in default eazel service user, prompting if necessary", NULL},
	{"status", 's', POPT_ARG_NONE, &opt_status, 0, "Print status", NULL},
	{"follow", 'f', POPT_ARG_NONE, &opt_follow, 0, "Status, follow (stay alive)", NULL},
	{"password", 'p', POPT_ARG_STRING, &opt_password, 0, "Password for user specified in -l", "<passwd>"},
	{"logout", 'u', POPT_ARG_STRING, &opt_logout, 0, "Logout user on port", "<port number>"},
	{"target-path", 't', POPT_ARG_STRING, &opt_target_path, 0, "Target path to redirect to", "<url>"},
	{"login-path", 'L', POPT_ARG_STRING, &opt_login_path, 0, "Path to login cgi (relative to target path)",
		"<path>"},
	{"change-password", 'c', POPT_ARG_NONE, &opt_chpw, 0, "Change password (prompted for username)", NULL},
	{"uri-translate", 'x', POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN, &opt_translate, 0, NULL, NULL},
#ifdef ENABLE_REAUTHN
	{"reauthn-listen", '\0', POPT_ARG_NONE  | POPT_ARGFLAG_DOC_HIDDEN, &opt_reauthn_listen, 0, NULL, NULL},
#endif /* ENABLE_REAUTHN */
	POPT_AUTOHELP
	{NULL, 'h', POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN, &opt_help, 0, NULL, NULL},
	{NULL, '\0', 0, NULL, 0, NULL}
};

static void do_login (EazelProxy_UserControl cntl, const char *login, const char *passwd, const char *target_path,
		      const char *login_path);
static void do_login_prompt (EazelProxy_UserControl cntl, const char *target_path, const char *login_path);
static void do_status (EazelProxy_UserControl cntl);
static void do_follow (EazelProxy_UserControl cntl);
static void do_logout (EazelProxy_UserControl cntl, const char *username);
static void do_translate (EazelProxy_UserControl cntl) ;
static void do_change_password (EazelProxy_UserControl cntl);

EazelProxy_User *
eazel_proxy_util_do_login (EazelProxy_UserControl cntl, const char *login, const char *passwd, const char *target_path,
	  const char *login_path, /*OUT*/ CORBA_long *p_fail_code);

#if 0
static EazelProxy_User *
eazel_proxy_util_do_prompt_login (
	EazelProxy_UserControl cntl, 
	const char *username,
	const char *target_path, 
	const char *login_path,
	/* OUT */ CORBA_long *p_fail_code
);
#endif /* 0 */

EazelProxy_User *
eazel_proxy_util_change_password (
	EazelProxy_UserControl cntl, 
	const char *login, 
	const char *old_passwd,
	const char *new_passwd
);


static void
illegal_options (poptContext context)
{
	
	fprintf (stderr, "Illegal combination of options specified\n");

	poptPrintUsage (context , stderr, 0);
}

int main (int argc, char **argv)
{
	CORBA_Environment ev;
	CORBA_ORB orb;
	poptContext popt_context;
	int rc;

	CORBA_exception_init (&ev);

	DEBUG_MSG ((stderr,"%lu: In eazel-proxy-util ppid %lu\n", (unsigned long) getpid(), (unsigned long)getppid()));

	orb = oaf_init (argc, argv);

	if (NULL == orb ) {
		fprintf (stderr, "Unable to init OAF\n");
		exit (-1);
	}

	/* Get the POA for further object activation */
	gl_poa = (PortableServer_POA)CORBA_ORB_resolve_initial_references(orb, "RootPOA", &ev);
	PortableServer_POAManager_activate(PortableServer_POA__get_the_POAManager(gl_poa, &ev), &ev);

	if (! ammonite_init(gl_poa) ) {
		fprintf (stderr, "Error during ammonite_init()\n");
		exit (-1);
	}
	gl_user_control = ammonite_get_user_control();

	popt_context = poptGetContext("eazel-proxy-control", argc, (const char **)argv, popt_options, 0);

	if (-1 != (rc = poptGetNextOpt (popt_context) ) ) {
		exit (-1);
	}

	if (opt_help) {
		poptPrintUsage (popt_context, stderr, 0);
		exit (0);
	}

	/* mfleming -- this is no longer an accurate representation of valid combinations */
	if ( opt_login && ( opt_status || opt_follow || opt_logout || opt_translate ) ) {
		illegal_options(popt_context);
		exit (-1);
	} else if ( opt_status && ( opt_login || opt_follow || opt_password || opt_logout || opt_translate) ) {
		illegal_options(popt_context);
		exit (-1);
	} else if ( opt_follow && ( opt_login || opt_status || opt_password || opt_logout || opt_translate) ) {
		illegal_options(popt_context);
		exit (-1);
	} else if ( opt_password &&  !opt_login ) {
		illegal_options(popt_context);
		exit (-1);
	} else if ( opt_logout && ( opt_login || opt_status || opt_password || opt_follow || opt_translate) ) {
		illegal_options(popt_context);
		exit (-1);
	} else if ( opt_translate && ( opt_login || opt_status || opt_password || opt_follow || opt_logout) ) {
		illegal_options(popt_context);
		exit (-1);
	}

	if (opt_login) {
		do_login (gl_user_control, opt_login, opt_password, opt_target_path, opt_login_path);
	} else if (opt_login_prompt) {
		do_login_prompt (gl_user_control, opt_target_path, opt_login_path);
	} else if (opt_status) {
		do_status (gl_user_control);
	} else if (opt_follow) {
		do_follow (gl_user_control);
	} else if (opt_logout) {
		do_logout (gl_user_control, opt_logout);
	} else if (opt_translate) {
		do_translate (gl_user_control) ;
#if 0
	} else if (opt_dialog) {
		eazel_proxy_util_do_dialog ();
#endif /* 0 */
	} else if (opt_chpw) {
		do_change_password (gl_user_control);
#ifdef ENABLE_REAUTHN
	} else if (opt_reauthn_listen) {
		eazel_proxy_util_do_reauthn_listen (gl_user_control);
#endif /* ENABLE_REAUTHN */
	}
	
	poptFreeContext (popt_context);

	CORBA_Object_release (gl_user_control, &ev);

	return 0;	
}

#if 0
/* This hack is necessary to ensure that all
 * oneway functions get sent to the CORBA object in question
 * before this application quits.
 * 
 * It seems like this must be a bug in ORBit or a side-effect
 * of the fact that I'm not using glib's event loop
 */

static void
corba_round_trip (EazelProxy_UserControl cntl)
{
	CORBA_Environment ev;
	EazelProxy_User *user;

	CORBA_exception_init (&ev);

	user = EazelProxy_UserControl_get_default_user (cntl, &ev);

	if ( CORBA_NO_EXCEPTION == ev._major ) {
		CORBA_free (user);
	}

	CORBA_exception_free (&ev);
}
#endif

typedef struct {
	gboolean done;
	gboolean success;

	CORBA_long code;
	EazelProxy_User *user;
} LoginSignalState;

static void
authn_cb_succeeded (const EazelProxy_User *user, gpointer state, CORBA_Environment *ev)
{
	LoginSignalState *my_state;

	DEBUG_MSG ((stderr, "%lu: In authn_cb_succeeded\n", (unsigned long) getpid()));
	
	g_assert (NULL != user);
	g_assert (NULL != state);

	my_state = (LoginSignalState *)state;

	my_state->done = TRUE;
	my_state->success = TRUE;

	my_state->user = EazelProxy_User_duplicate (user);

}

static void
authn_cb_failed (const EazelProxy_User *user, const EazelProxy_AuthnFailInfo *info, gpointer state, CORBA_Environment *ev)
{
	LoginSignalState *my_state;
	
	g_assert (NULL != user);
	g_assert (NULL != state);

	DEBUG_MSG ((stderr, "%lu: In authn_cb_failed\n", (unsigned long) getpid()));

	my_state = (LoginSignalState *)state;

	my_state->done = TRUE;
	my_state->success = FALSE;
	my_state->code = info->code;
}

static void
do_login (EazelProxy_UserControl cntl, const char *login, const char *passwd, const char *target_path,
	  const char *login_path)
{
	char * read_passwd = NULL;
	EazelProxy_User *user;

	if (NULL == passwd ) {
		char *prompt;

		prompt = g_strdup_printf ("Password for user '%s':", login);
		read_passwd = getpass (prompt);
		g_free (prompt);
	}

	user = eazel_proxy_util_do_login (cntl, login,  
				NULL == passwd ? read_passwd : passwd, 
				target_path, login_path, NULL
	       );

	g_free (read_passwd);

	if ( NULL != user ) {
		printf ("Login succeeded.\n");
		CORBA_free (user);
	} else {
		printf ("Login FAILED.\n");
	}
}

static void
do_login_prompt (
	EazelProxy_UserControl cntl, 
	const char *target_path, 
	const char *login_path
) {
	EazelProxy_User *user;
	
	user = ammonite_do_prompt_login (NULL, target_path, login_path, NULL);

	if ( NULL != user ) {
		printf ("Login succeeded for user '%s' on port %d.\n", 
			user->user_name, 
			(int)user->proxy_port
		);
		CORBA_free (user);
	} else {
		printf ("Login FAILED.\n");
	}
}

EazelProxy_User *
eazel_proxy_util_do_login (EazelProxy_UserControl cntl, const char *login, const char *passwd, const char *target_path,
	  const char *login_path, /*OUT*/ CORBA_long *p_fail_code)
{
	CORBA_Environment ev;
	EazelProxy_AuthnCallback authn_callback;
	volatile LoginSignalState state;
	EazelProxy_AuthnInfo *authinfo;
	EazelProxy_User *user = NULL;

	AmmoniteAuthCallbackWrapperFuncs cb_funcs = {
		authn_cb_succeeded, authn_cb_failed
	};

	CORBA_exception_init (&ev);

	g_return_val_if_fail (NULL != login, NULL);
	g_return_val_if_fail (NULL != passwd, NULL);

	authn_callback = ammonite_auth_callback_wrapper_new (gl_poa, &cb_funcs, (gpointer)&state);

	if (CORBA_OBJECT_NIL == authn_callback) {
		fprintf (stderr, "Couldn't create AuthnCallback\n");
		exit (-1);
	}

	memset ((void *)&state, 0, sizeof(state) );

	authinfo = EazelProxy_AuthnInfo__alloc ();
	authinfo->username = CORBA_string_dup (login);
	authinfo->password = CORBA_string_dup (passwd);
	authinfo->services_redirect_uri = CORBA_string_dup (target_path ? target_path : "");
	authinfo->services_login_path = CORBA_string_dup (login_path ? login_path : "");

	EazelProxy_UserControl_authenticate_user (cntl, authinfo, TRUE, authn_callback, &ev);

	CORBA_free (authinfo);

	if (CORBA_NO_EXCEPTION != ev._major) {
		fprintf (stderr, "Exception during authenticate_user\n");
	} else {
		while ( ! state.done && ! CORBA_Object_non_existent (cntl, &ev) ) {
			g_main_iteration(TRUE);
		}

		if (state.success) {
			user = state.user;
		} else if (p_fail_code) {
			*p_fail_code = state.code;
		}
	}

	ammonite_auth_callback_wrapper_free (gl_poa, authn_callback);

	CORBA_exception_free (&ev);

	return user;
}

#if 0

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
		eazel_proxy_util_do_error_dialog();

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
			ret = eazel_proxy_util_do_prompt_dialog (NULL, NULL, kind == EazelProxy_Initial, &username_glib, &password_glib);
		} else {
			ret = eazel_proxy_util_do_prompt_dialog (user->user_name, NULL, kind == EazelProxy_Initial, NULL, &password_glib);
		}

		*authninfo = EazelProxy_AuthnInfo__alloc();

		(*authninfo)->services_redirect_uri = CORBA_string_dup ("");
		(*authninfo)->services_login_path = CORBA_string_dup ("");

		if (ret) {
			(*authninfo)->username = CORBA_string_dup ( (NULL != username_glib) ? username_glib : "" ); 
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

static EazelProxy_User *
eazel_proxy_util_do_prompt_login (
	EazelProxy_UserControl cntl,
	const char *username, 
	const char *target_path, 
	const char *login_path,
	/*OUT*/ CORBA_long *p_fail_code
) {

	CORBA_Environment ev;
	EazelProxy_UserPrompter userprompter;
	EazelProxy_AuthnCallback authn_callback;
	volatile LoginSignalState state;
	EazelProxy_AuthnInfo *authinfo;
	EazelProxy_User *user = NULL;

	AmmoniteAuthCallbackWrapperFuncs authn_cb_funcs = {
		authn_cb_succeeded, authn_cb_failed
	};
	AmmoniteUserPrompterWrapperFuncs up_cb_funcs = {
		prompter_cb_prompt_authenticate
	};

	CORBA_exception_init (&ev);

	userprompter = ammonite_userprompter_wrapper_new (gl_poa, &up_cb_funcs, (gpointer)&state);
	authn_callback = ammonite_auth_callback_wrapper_new (gl_poa, &authn_cb_funcs, (gpointer)&state);

	if (CORBA_OBJECT_NIL == authn_callback) {
		fprintf (stderr, "Couldn't create AuthnCallback\n");
		exit (-1);
	}

	if (CORBA_OBJECT_NIL == userprompter) {
		fprintf (stderr, "Couldn't create UserPrompter\n");
		exit (-1);
	}

	memset ((void *)&state, 0, sizeof(state) );

	authinfo = EazelProxy_AuthnInfo__alloc ();
	authinfo->username = CORBA_string_dup (NULL == username ? "" : username);
	authinfo->password = CORBA_string_dup ("");
	authinfo->services_redirect_uri = CORBA_string_dup (target_path ? target_path : "");
	authinfo->services_login_path = CORBA_string_dup (login_path ? login_path : "");

	DEBUG_MSG ((stderr, "%lu: Calling prompt_authenticate\n", (unsigned long) getpid()));

	EazelProxy_UserControl_prompt_authenticate_user (cntl, authinfo, TRUE, userprompter, authn_callback, &ev);

	DEBUG_MSG ((stderr, "%lu: Back from prompt_authenticate\n", (unsigned long) getpid()));

	CORBA_free (authinfo);

	if (CORBA_SYSTEM_EXCEPTION == ev._major) {
		DEBUG_MSG ((stderr, "%lu: CORBA system exception; exiting\n",(unsigned long) getpid()));
		exit (-1);
	} else if (CORBA_NO_EXCEPTION != ev._major) {
		fprintf (stderr, "Exception during prompt_authenticate_user\n");
	} else {
		while ( ! state.done && ! CORBA_Object_non_existent (cntl, &ev) ) {
			g_main_iteration(TRUE);
		}

		if (CORBA_Object_non_existent (cntl, &ev)) {
			DEBUG_MSG ((stderr, "%lu: CORBA server disappeared; exiting\n",(unsigned long) getpid()));
			exit (-1);
		}

		if (state.success) {
			user = state.user;
		} else if (p_fail_code) {
			*p_fail_code = state.code;
		}

		DEBUG_MSG ((stderr, "%lu: Response iteration complete, success=%d\n", (unsigned long) getpid(), state.success));
	}

	if ( ! state.success 
		  && ( EAZELPROXY_AUTHN_FAIL_NETWORK == state.code
		  		|| EAZELPROXY_AUTHN_FAIL_SERVER == state.code
		  )
	) {
		eazel_proxy_util_do_network_error_dialog();
	}

	ammonite_auth_callback_wrapper_free (gl_poa, authn_callback);
	ammonite_userprompter_wrapper_free (gl_poa, userprompter);

	CORBA_exception_free (&ev);

	return user;

}
#endif /*0*/

static const char *
LoginState_to_string (EazelProxy_LoginState state )
{
	static const char * loginStates[] = {
		"Unauthned", "Authning", "Authned"
	};

	if (state > (sizeof (loginStates) / sizeof (loginStates[0]) ) 
		|| state < 0
	) {
		return NULL;
	} else {
		return loginStates [state];
	}
}

static void
do_status (EazelProxy_UserControl cntl)
{

	CORBA_Environment ev;
	EazelProxy_UserList *users;
	CORBA_unsigned_long i;
	int showed_default = 0;

	CORBA_exception_init (&ev);

	users = EazelProxy_UserControl_get_active_users (cntl, &ev);

	if (CORBA_NO_EXCEPTION != ev._major) {
		fprintf (stderr, "Exception during authenticate_user\n");
		return;
	}

	printf ("%c %-8s %-10s %-6s %s\n", ' ', "Username", "State", "Port", "Target Path");
	for (i = 0; i < users->_length ; i++) {
		EazelProxy_User *cur;

		cur = users->_buffer + i;
		printf ("%c %-8s %-10s %-6d %s\n",
			cur->is_default ? '*' : ' ',
			cur->user_name,
			LoginState_to_string (cur->login_state),
			cur->proxy_port,
			cur->services_redirect_uri
		);
		if (cur->services_login_path[0]) {
			printf ("\t(login: %s)\n", cur->services_login_path);
		}
		if (cur->is_default) {
			showed_default = 1;
		}
	}

	if (showed_default) {
		printf("\n(* default user login)\n");
	}

	CORBA_free (users);
	CORBA_exception_free (&ev);

}

static void
user_listener_user_authenticated (const EazelProxy_User *user, gpointer user_data, CORBA_Environment *ev)
{
	printf ("AUTHN: User '%s' on port %d\n", user->user_name, user->proxy_port);
}

static void
user_listener_user_authenticate_failed (const EazelProxy_User *user, const EazelProxy_AuthnFailInfo *info, gpointer user_data, CORBA_Environment *ev)
{
	printf ("AUTHNFAIL: User '%s' on port %d\n", user->user_name, user->proxy_port);
}

static void
user_logout (const EazelProxy_User *user, gpointer user_data, CORBA_Environment *ev)
{
	printf ("LOGOUT: User '%s' on port %d\n", user->user_name, user->proxy_port);
}

static GMainLoop * gl_follow_main_loop = NULL;

static void
follow_handle_signal(int sig)
{
	switch (sig) {
	case SIGINT:
	case SIGHUP:
	case SIGTERM:
		g_main_quit( gl_follow_main_loop );
		break;
	}
}

static void
do_follow (EazelProxy_UserControl cntl)
{

	CORBA_Environment ev;
	EazelProxy_UserListener user_listener;

	AmmoniteUserListenerWrapperFuncs cb_funcs = {
		user_listener_user_authenticated,
		user_listener_user_authenticate_failed,
		user_logout
	};

	CORBA_exception_init (&ev);

	user_listener = ammonite_user_listener_wrapper_new (gl_poa, &cb_funcs, NULL);

	if (CORBA_OBJECT_NIL == user_listener) {
		fprintf (stderr, "Couldn't create AuthnCallback\n");
		exit (-1);
	}

	EazelProxy_UserControl_add_listener (cntl, user_listener, &ev);

	if (CORBA_NO_EXCEPTION != ev._major) {
		fprintf (stderr, "Exception during add_listener\n");
	} else {
		void * old_sigterm, * old_sighup, *old_sigint;
		
		old_sigint = signal(SIGINT, follow_handle_signal);
		old_sigterm = signal(SIGTERM, follow_handle_signal);
		old_sighup = signal(SIGHUP, follow_handle_signal);

		gl_follow_main_loop = g_main_new (FALSE);

		g_main_run (gl_follow_main_loop);		

		g_main_destroy (gl_follow_main_loop);

		gl_follow_main_loop = NULL;		

		signal(SIGINT, old_sigint);
		signal(SIGTERM, old_sigterm);
		signal(SIGHUP, old_sighup);
	}

	EazelProxy_UserControl_remove_listener (cntl, user_listener, &ev);

	ammonite_user_listener_wrapper_free (gl_poa, user_listener);

	CORBA_exception_free (&ev);

}

static void
do_logout (EazelProxy_UserControl cntl, const char *user_port)
{
	CORBA_Environment ev;
	CORBA_boolean success = FALSE;
	CORBA_unsigned_short port;
	char *user_port_end;

	CORBA_exception_init (&ev);

	port = strtoul (user_port, &user_port_end, 10);

	if (user_port == user_port_end) {
		fprintf (stderr, "You must specify a port number to log out\n");
		exit (-1);
	}

	success = EazelProxy_UserControl_logout_user (cntl, port, &ev);

	if (CORBA_NO_EXCEPTION != ev._major) {
		fprintf (stderr, "Exception during authenticate_user\n");
	}

	if (success) {
		printf ("Logout '%d' succeeded\n", (int)port);
	} else {
		printf ("Logout '%d' FAILED\n", (int)port);
	}

	CORBA_exception_free (&ev);
}

#define GETLINE_DELTA 256
static char * my_getline (FILE* stream)
{
	char *ret;
	size_t ret_size;
	size_t ret_used;
	int char_read;
	gboolean got_newline;

	ret = g_malloc( GETLINE_DELTA * sizeof(char) );
	ret_size = GETLINE_DELTA;

	for ( ret_used = 0, got_newline = FALSE ;
	      !got_newline && (EOF != (char_read = fgetc (stream))) ; 
	      ret_used++
	) {
		if (ret_size == (ret_used + 1)) {
			ret_size += GETLINE_DELTA;
			ret = g_realloc (ret, ret_size); 
		}
		if ('\n' == char_read || '\r' == char_read ) {
			got_newline = TRUE;
			ret [ret_used] = '\0';
		} else {
			ret [ret_used] = char_read;
		}
	}

	if (got_newline) {
		return ret;
	} else {
		g_free (ret);
		return NULL;
	}
}


#if 0
static char *
make_new_uri (EazelProxy_User *user_info, const AuthProxyURI *parsed)
{
	g_return_val_if_fail ( NULL != user_info, NULL );
	g_return_val_if_fail ( NULL != parsed, NULL );

	if (EazelProxy_AUTHENTICATED == user_info->login_state 
	    && 0 != user_info->proxy_port
	) {
		return g_strdup_printf ("//localhost:%d%s", user_info->proxy_port, parsed->resource );
	} else {
		return NULL;
	}
}
#endif


#if 0
/* FIXME: bugzilla.eazel.com 2850: should cache the result and follow with a Listener */
/* FIXME: bugzilla.eazel.com 2850: should support realms */
static EazelProxy_User *
usercontrol_find_user (EazelProxy_UserControl cntl, const char *user, const char *realm)
{
	CORBA_Environment ev;
	EazelProxy_User *ret = NULL;
	EazelProxy_UserList *userlist = NULL;
	CORBA_unsigned_long i;

	CORBA_exception_init (&ev);

	userlist = EazelProxy_UserControl_get_active_users (cntl, &ev);

	if (CORBA_SYSTEM_EXCEPTION == ev._major) {
		DEBUG_MSG ((stderr, "%lu: CORBA system exception; exiting\n",(unsigned long) getpid()));
		exit (-1);
	}

	if ( NULL == userlist || CORBA_NO_EXCEPTION != ev._major ) {
		goto error;
	}

	for (i = 0 ; i < userlist->_length ; i++ ) {
		EazelProxy_User *current;

		current = userlist->_buffer + i;
		if ( NULL != current->user_name
			&& 0 == strcmp (current->user_name, user)
		) {
			ret = EazelProxy_User_duplicate (current);
			break;
		} 
	} 

error:
	CORBA_free (userlist);
	CORBA_exception_free (&ev);

	return ret;
}
#endif

static gboolean
try_init_gnome ()
{
	static gboolean is_gnome_inited = FALSE;

	if (!is_gnome_inited) {
		char * my_argv[] = {"eazel-proxy-util", NULL};
		int err;
		GnomeClient *session_client;

#ifdef DEBUG
		fprintf (stderr, "Initing GNOME\n");
#endif /* DEBUG */

		if (NULL == g_getenv ("DISPLAY")) {
#ifdef DEBUG
			fprintf (stderr, "No DISPLAY variable, not opening login dialog\n");
#endif
			return FALSE;
		}

		err = gnome_init("eazel-proxy-util", VERSION, 1, my_argv);

		if ( 0 != err ) {
#ifdef DEBUG
			fprintf (stderr, "couldn't init GNOME\n");
#endif
			return FALSE;
		}

		is_gnome_inited = TRUE;

		/* Disconnect from the session manager so we don't get 
		 * annoying "SaveYourself" dialogs
		 */
		session_client = gnome_master_client ();
		if (session_client) {
			gnome_client_disconnect (session_client);
		}
	}

	return TRUE;
}


static void
do_translate (EazelProxy_UserControl cntl)
{
	char * line;
	gboolean gnome_inited;

	setvbuf (stdin, NULL, _IOLBF, 0);
	setvbuf (stdout, NULL, _IOLBF, 0);

	gnome_inited = try_init_gnome();

	for ( line = NULL ; NULL != (line = my_getline(stdin)) ; g_free(line) ) {
		static time_t last_fail_time	= 0;
		AmmoniteParsedURL *parsed 	= NULL;
		EazelProxy_User *user_info 	= NULL;
		char *orig_url			= NULL;
		char *new_url 			= NULL;
		AmmoniteError err;

		DEBUG_MSG ((stderr, "%lu: read uri: '%s'\n", (unsigned long) getpid(), line));

		orig_url = g_strconcat (":", line, NULL);
		g_free (line);
		line = orig_url;
		orig_url = NULL;

		err = ammonite_http_url_for_eazel_url (line, &new_url);


		if (ERR_BadURL == err) {
			printf (":\n");
			continue;
		} else if (ERR_CORBA == err) {
			DEBUG_MSG ((stderr, "%lu: CORBA error, exiting", (unsigned long) getpid()));
			exit (-1);
		} else if (ERR_UserNotLoggedIn == err) {
			/* If authentication fails--for whatever reason--ignore subsequent requests for a few seconds
			 * so we don't get redundent login boxes from the same operation
			 */
			if (time(NULL) - last_fail_time <= IGNORE_LOGIN_AFTER_FAIL_SECONDS) {
				DEBUG_MSG ((stderr, "%lu: Ignoring subsequent login attempt--too soon since last fail\n",(unsigned long) getpid()));
				printf (":\n");
				continue;
			}

			parsed = ammonite_url_parse (line);
			if (parsed && gnome_inited) {
				user_info = ammonite_do_prompt_login (parsed->user, NULL, NULL, NULL);
				if (user_info) {
					err = ammonite_http_url_for_eazel_url (line, &new_url);
					/* I'm just not going to deal with this unlikely fail case */
					if (ERR_Success != err) {
						printf (":\n");
						continue;
					}
					
					CORBA_free (user_info);

				} else {
					last_fail_time = time(NULL);
					printf (":\n");
					continue;
				}
				ammonite_url_free (parsed);
			} else {
				printf (":\n");
				continue;
			}
		}

		/* skip ":" on new_url */
		printf ("%s\n", new_url + 1);
		DEBUG_MSG ((stderr, "%lu: wrote uri: '%s'\n", (unsigned long) getpid(), new_url+1));
		g_free (new_url);

	}

	DEBUG_MSG ((stderr, "%lu: leaving do_translate\n", (unsigned long) getpid()));
}

EazelProxy_User *
eazel_proxy_util_change_password (
	EazelProxy_UserControl cntl, 
	const char *login, 
	const char *old_passwd,
	const char *new_passwd
) {
	CORBA_Environment ev;
	EazelProxy_AuthnCallback authn_callback;
	volatile LoginSignalState state;
	EazelProxy_AuthnInfo *authinfo;
	EazelProxy_User *user = NULL;

	AmmoniteAuthCallbackWrapperFuncs cb_funcs = {
		authn_cb_succeeded, authn_cb_failed
	};

	CORBA_exception_init (&ev);

	g_return_val_if_fail (NULL != login, NULL);
	g_return_val_if_fail (NULL != old_passwd, NULL);
	g_return_val_if_fail (NULL != new_passwd, NULL);

	authn_callback = ammonite_auth_callback_wrapper_new (gl_poa, &cb_funcs, (gpointer)&state);

	if (CORBA_OBJECT_NIL == authn_callback) {
		fprintf (stderr, "Couldn't create AuthnCallback\n");
		exit (-1);
	}

	memset ((void *)&state, 0, sizeof(state) );

	authinfo = EazelProxy_AuthnInfo__alloc ();
	authinfo->username = CORBA_string_dup (login);
	authinfo->password = CORBA_string_dup (old_passwd);
	authinfo->services_redirect_uri = CORBA_string_dup ("");
	authinfo->services_login_path = CORBA_string_dup ("");

	EazelProxy_UserControl_set_new_user_password (cntl, authinfo, new_passwd, authn_callback, &ev);

	CORBA_free (authinfo);

	if (CORBA_NO_EXCEPTION != ev._major) {
		fprintf (stderr, "Exception during authenticate_user\n");
	} else {
		while ( ! state.done && ! CORBA_Object_non_existent (cntl, &ev) ) {
			g_main_iteration(TRUE);
		}

		if (state.success) {
			user = state.user;
		}
	}

	ammonite_auth_callback_wrapper_free (gl_poa, authn_callback);

	CORBA_exception_free (&ev);

	return user;
}

#define USERNAME_SIZE 256
static void
do_change_password (EazelProxy_UserControl cntl)
{
	char * username = NULL;
	char * orig_passwd = NULL;
	char * new_passwd0 = NULL;
	char * new_passwd1 = NULL;
	char *prompt = NULL;
	EazelProxy_User *user;

	username = g_malloc (USERNAME_SIZE * sizeof (char));
	printf ("Username: ");
	username = g_strchomp (fgets (username, USERNAME_SIZE, stdin));

	prompt = g_strdup_printf ("Original password for user '%s':", username);
	orig_passwd = g_strdup (getpass (prompt));
	g_free (prompt);

	new_passwd0 = g_strdup (getpass ("New Password:"));
	new_passwd1 = g_strdup (getpass ("Please retype new Password:"));

	if (0 != strcmp (new_passwd0, new_passwd1)) {
		printf ("New password entries do not match.  Password not changed.\n");
		exit (-1);
	}

	user = eazel_proxy_util_change_password (cntl, username,  
				orig_passwd, new_passwd0
	       );

	if ( NULL != user ) {
		printf ("Change password succeeded.\n");
		CORBA_free (user);
	} else {
		printf ("Change password FAILED.\n");
	}
}

