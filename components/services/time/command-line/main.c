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
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 *
 */

#include <config.h>
#include <gnome.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>
#include <unistd.h>		/* for getpass() */

#include <libtrilobite/libtrilobite.h>
#include <libtrilobite/libtrilobite-service.h>

#include "trilobite-eazel-time-service.h"

#define OAF_ID "OAFIID:trilobite_eazel_time_service:13a2dbd9-84f9-4400-bd9e-bb4575b86894"

int     arg_list_info,
	arg_max_diff,
	arg_update_time;
char   *arg_url;

static const struct poptOption options[] = {
	{"info", 'i', POPT_ARG_NONE, &arg_list_info, 0, N_("display service name and such"), NULL},
	{"maxdiff", '\0', POPT_ARG_INT, &arg_max_diff, -1, N_("maximum allowed difference in seconds"), NULL},
	{"update", 'u', POPT_ARG_NONE, &arg_update_time, 0, N_("update the system clock"), NULL},
	{"url", '\0', POPT_ARG_STRING, &arg_url, 0, N_("specify time url"), NULL},
	{NULL, '\0', 0, NULL, 0}
};

CORBA_Environment ev;
CORBA_ORB orb;


static char *
get_password (GtkObject *object, const char *prompt)
{
	char * real_prompt;
	char * passwd;

	real_prompt = g_strdup_printf ("%s: ", prompt);
	passwd = getpass (real_prompt);
	g_free (real_prompt);

	return g_strdup (passwd);
}


/* This is basically ripped from empty-client */

int main(int argc, char *argv[]) {
	BonoboObjectClient *service;
	Trilobite_Eazel_Time timeservice;
	CORBA_long diff;
	TrilobiteRootClient *root_client;

	CORBA_exception_init (&ev);
	gnomelib_register_popt_table (oaf_popt_options, "OAF options");
	gnome_init_with_popt_table ("trilobite-eazel-time-service-cli", "1.0",argc, argv, options, 0, NULL);
	orb = oaf_init (argc, argv);
	
	if (bonobo_init (NULL, NULL, NULL) == FALSE) {
		g_error ("Could not init bonobo");
	}
	bonobo_activate ();

	service = bonobo_object_activate (OAF_ID, 0);
	if (!service) {
		g_error ("Cannot activate %s\n",OAF_ID);
	}	

	if (! bonobo_object_client_has_interface (service, "IDL:Trilobite/Service:1.0", &ev)) {
		g_error ("Object does not support IDL:/Trilobite/Service:1.0");
	}
	if (! bonobo_object_client_has_interface (service, "IDL:Trilobite/Eazel/Time:1.0", &ev)) {
		g_error ("Object does not support IDL:/Trilobite/Eazel/Time:1.0");
	}

	/* If a trilobite, get the interface and dump the info */
	if (arg_list_info) {
		Trilobite_Service trilobite;
		trilobite = bonobo_object_query_interface (BONOBO_OBJECT (service), "IDL:Trilobite/Service:1.0");
		g_message ("service name        : %s", Trilobite_Service_get_name (trilobite, &ev));
		g_message ("service version     : %s", Trilobite_Service_get_version (trilobite, &ev));
		g_message ("service vendor name : %s", Trilobite_Service_get_vendor_name (trilobite, &ev));
		g_message ("service vendor url  : %s", Trilobite_Service_get_vendor_url (trilobite, &ev));
		g_message ("service url         : %s", Trilobite_Service_get_url (trilobite, &ev));
		g_message ("service icon        : %s", Trilobite_Service_get_icon (trilobite, &ev));		
		Trilobite_Service_unref (trilobite, &ev);
		CORBA_Object_release (trilobite, &ev);
	} 

	/* set callbacks to get the root password */
	root_client = trilobite_root_client_new ();
	trilobite_root_client_attach (root_client, service);
	gtk_signal_connect (GTK_OBJECT (root_client), "need_password",
			    GTK_SIGNAL_FUNC (get_password), NULL);

	timeservice = bonobo_object_query_interface (BONOBO_OBJECT (service), "IDL:Trilobite/Eazel/Time:1.0");

	if (arg_max_diff>=0) {
		Trilobite_Eazel_Time_set_max_difference (timeservice, arg_max_diff, &ev);
	}

	if (arg_url) {
		Trilobite_Eazel_Time_set_time_url (timeservice, arg_url, &ev);
	} 

	diff = Trilobite_Eazel_Time_check_time (timeservice, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		if (strcmp (ex_Trilobite_Eazel_Time_CannotGetTime, CORBA_exception_id (&ev)) == 0) {
			Trilobite_Eazel_Time_CannotGetTime *exn; 
			fprintf (stderr, "Unable to obtain time from server\n");
			exn = (Trilobite_Eazel_Time_CannotGetTime*)CORBA_exception_value (&ev);
			fprintf (stderr, "URL was %s\nReason is %s\n", exn->url, exn->reason);
		} else {
			fprintf (stderr, "Unhandleable error occured while communicating with the time-service\n");
			fprintf (stderr, "Caught %s\n", CORBA_exception_id (&ev));
		}
		arg_update_time = 0;
		CORBA_exception_free (&ev);
	} else {
		if (diff != 0) {
			fprintf (stdout, "Time mismatch (%d seconds %s), %s\n", 
				 abs (diff), 
				 diff < 0 ? "behind" : "ahead",
				 arg_update_time ? "will update." : "suggest you update the time.");
		} else {
			fprintf (stdout, "Time matches server time\n");
		}
	}

	if (arg_update_time) {
		Trilobite_Eazel_Time_update_time (timeservice, &ev);
		if (ev._major == CORBA_USER_EXCEPTION) {
			if (strcmp (ex_Trilobite_Eazel_Time_NotPermitted, CORBA_exception_id (&ev)) == 0) {
				fprintf (stderr, "You are not permitted to change system time\n");
			}
			CORBA_exception_free (&ev);
		}
	}

	trilobite_root_client_unref (GTK_OBJECT (root_client));
	Bonobo_Unknown_unref (timeservice, &ev);
	CORBA_Object_release (timeservice, &ev);
	bonobo_object_unref (BONOBO_OBJECT (service)); 
	CORBA_exception_free (&ev);

	return 0;
};
