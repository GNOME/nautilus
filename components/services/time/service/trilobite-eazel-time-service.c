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

#include <libtrilobite/libtrilobite.h>
#include <libtrilobite/libtrilobite-service.h>

#include "trilobite-eazel-time-service.h"
#include "trilobite-eazel-time-service-public.h"
#include "trilobite-eazel-time-service-private.h"

#include <ghttp.h>
#include <time.h>

/* might wanna make this configable */
/* # of seconds to wait between bugging the time server again */
#define TIME_SERVER_CACHE_TIMEOUT	60


/* This is the parent class pointer */
static GtkObjectClass *trilobite_eazel_time_service_parent_class;

/* prototypes */

time_t trilobite_eazel_time_service_get_server_time (TrilobiteEazelTimeService*, CORBA_Environment *ev);

/*****************************************
  Corba stuff
*****************************************/

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

typedef struct {
	POA_Trilobite_Eazel_Time poa;
	TrilobiteEazelTimeService *object;
} impl_POA_Trilobite_Eazel_Time_Service;

static void
impl_Trilobite_Eazel_Time_Service_set_max_difference (impl_POA_Trilobite_Eazel_Time_Service *service,
						      const CORBA_long maxd,
						      CORBA_Environment *ev) 
{
	service->object->private->maxd = maxd;
	service->object->private->time_obtained = 0;
}

static void
impl_Trilobite_Eazel_Time_Service_set_time_url (impl_POA_Trilobite_Eazel_Time_Service *service,
						const CORBA_char *url,
						CORBA_Environment *ev) 
{
	if (service->object->private->time_url != NULL) {
		g_free (service->object->private->time_url);
	}
	service->object->private->time_url = g_strdup (url);
	service->object->private->time_obtained = 0;
}

static CORBA_unsigned_long
impl_Trilobite_Eazel_Time_Service_check_time  (impl_POA_Trilobite_Eazel_Time_Service *service,
					       CORBA_Environment *ev) 
{
	time_t server_time;
	time_t local_time;
	CORBA_unsigned_long result;

	result = 0;
	local_time = time (NULL);
	server_time = trilobite_eazel_time_service_get_server_time (service->object, ev);

#if 0
	g_message ("Local time  : %ld", local_time);
	g_message ("Server time : %ld", server_time);
	g_message ("Diff is     : %d", abs (server_time - local_time));
	g_message ("Allowed d   : %d", service->object->private->maxd);
#endif
	if (server_time == 0) {
		g_warning ("Unable to get server time");
	} else if (abs (server_time - local_time) > service->object->private->maxd) {
		g_warning ("Time off by %d, max allowed diff is %d",
			   abs (server_time - local_time),
			   service->object->private->maxd);
	}

	/* If we did not get the time, raise an exception */
	if (server_time != 0) {
		/* if we are beyond the max difference, return it */
		if (abs (server_time - local_time) > service->object->private->maxd) {
			result = server_time - local_time;
		}
	}

	return result;
}

static void
impl_Trilobite_Eazel_Time_Service_update_time  (impl_POA_Trilobite_Eazel_Time_Service *service,
						CORBA_Environment *ev) 
{
	time_t server_time;
	TrilobiteRootHelper *root_helper;
	GList *args;
	char *tmp;

	root_helper = gtk_object_get_data (GTK_OBJECT (service->object), "trilobite-root-helper");
	if (trilobite_root_helper_start (root_helper) != TRILOBITE_ROOT_HELPER_SUCCESS) {
		Trilobite_Eazel_Time_NotPermitted *exn; 
		exn = Trilobite_Eazel_Time_NotPermitted__alloc ();
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_Trilobite_Eazel_Time_NotPermitted, exn);
		return;
	}

	server_time = trilobite_eazel_time_service_get_server_time (service->object, ev);
	if (server_time == 0) {
		return;
	}

	tmp = g_strdup_printf ("%ld", server_time);
	args = g_list_prepend (NULL, tmp);

	if (trilobite_root_helper_run (root_helper, TRILOBITE_ROOT_HELPER_RUN_SET_TIME, args, NULL) != TRILOBITE_ROOT_HELPER_SUCCESS) {
		Trilobite_Eazel_Time_NotPermitted *exn; 
		exn = Trilobite_Eazel_Time_CannotSet__alloc ();
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_Trilobite_Eazel_Time_CannotSet, exn);
	}

	g_list_free (args);
	g_free (tmp);
}

POA_Trilobite_Eazel_Time__epv* 
trilobite_eazel_time_service_get_epv () 
{
	POA_Trilobite_Eazel_Time__epv *epv;

	epv = g_new0 (POA_Trilobite_Eazel_Time__epv, 1);

	epv->set_max_difference = (gpointer) &impl_Trilobite_Eazel_Time_Service_set_max_difference;
	epv->set_time_url       = (gpointer) &impl_Trilobite_Eazel_Time_Service_set_time_url;
	epv->check_time         = (gpointer) &impl_Trilobite_Eazel_Time_Service_check_time;
	epv->update_time        = (gpointer) &impl_Trilobite_Eazel_Time_Service_update_time;
		
	return epv;
};

/*****************************************
  GTK+ object stuff
*****************************************/

/* This is the object destroyer. It should clean up any
 data allocated by the object, and if possible, call 
the parent destroyer */
void
trilobite_eazel_time_service_destroy (GtkObject *object)
{
	TrilobiteEazelTimeService *service;

	g_return_if_fail (object != NULL);
	g_return_if_fail (TRILOBITE_EAZEL_TIME_SERVICE (object));

	service = TRILOBITE_EAZEL_TIME_SERVICE (object);

	if (service->private->time_url) {
		g_free (service->private->time_url);
	}

	g_free (service->private);

	/* Call parents destroy */
	if (GTK_OBJECT_CLASS (trilobite_eazel_time_service_parent_class)->destroy) {
		GTK_OBJECT_CLASS (trilobite_eazel_time_service_parent_class)->destroy (object);
	}
}

/*
  This is the trilobite_eazel_time_service class initializer, see
  GGAD (http://developer.gnome.org/doc/GGAD/sec-classinit.html)
  for more on these
 */
static void
trilobite_eazel_time_service_class_initialize (TrilobiteEazelTimeServiceClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*)klass;
	object_class->destroy = (void(*)(GtkObject*))trilobite_eazel_time_service_destroy;

	trilobite_eazel_time_service_parent_class = gtk_type_class (gtk_object_get_type ());

	klass->servant_vepv = g_new0 (POA_Trilobite_Eazel_Time__vepv,1);
	((POA_Trilobite_Eazel_Time__vepv*)klass->servant_vepv)->_base_epv = &base_epv; 
	((POA_Trilobite_Eazel_Time__vepv*)klass->servant_vepv)->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	((POA_Trilobite_Eazel_Time__vepv*)klass->servant_vepv)->Trilobite_Eazel_Time_epv = trilobite_eazel_time_service_get_epv ();
}

/*
  _corba_object does all the greasy corba building and whatnot.
 */
static Trilobite_Eazel_Time
trilobite_eazel_time_service_create_corba_object (BonoboObject *service) {
	impl_POA_Trilobite_Eazel_Time_Service *servant;
	CORBA_Environment ev;

	/* g_message ("trilobite_eazel_time_service_create_corba_object"); */

	g_assert (service != NULL);
	g_assert (TRILOBITE_IS_EAZEL_TIME_SERVICE (service));
	
	CORBA_exception_init (&ev);

	servant = g_new0 (impl_POA_Trilobite_Eazel_Time_Service, 1);
	((POA_Trilobite_Eazel_Time *)servant)->vepv = TRILOBITE_EAZEL_TIME_SERVICE_CLASS ( GTK_OBJECT (service)->klass)->servant_vepv;
	POA_Trilobite_Eazel_Time__init (servant, &ev);
	ORBIT_OBJECT_KEY (((POA_Trilobite_Eazel_Time *)servant)->_private)->object = NULL;

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot instantiate Trilobite_Eazel_Time corba object");
		g_free (servant);
		CORBA_exception_free (&ev);		
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);		

	/* Return the bonobo activation of the servant */
	return (Trilobite_Eazel_Time) bonobo_object_activate_servant (service, servant);
}

/*
  This is the TrilobiteEazelTimeService instance initializer.
  Its responsibility is to create the corba object and 
  build the bonobo_object structures using the corba object.
 */
static void
trilobite_eazel_time_service_initialize (TrilobiteEazelTimeService *service) {
	Trilobite_Eazel_Time corba_service;

	/* g_message ("in trilobite_eazel_time_service_initialize"); */

	g_assert (service != NULL);
	g_assert (TRILOBITE_IS_EAZEL_TIME_SERVICE (service));

	/* This builds the corba object */
	corba_service = trilobite_eazel_time_service_create_corba_object (BONOBO_OBJECT (service));

	service->private = g_new0 (TrilobiteEazelTimeServicePrivate, 1);

	/* FIXME bugzilla.eazel.com 945:
	   These defaults should probably be read from somewhere */

	/* Default to the eazel test time server */
	service->private->time_url = g_strdup ("http://testmachine.eazel.com:8888/examples/time/current");

	/* Default to 180 seconds allowed diff */
	service->private->maxd = 180;

	/* This sets the bonobo structures in service using the corba object */
	if (!bonobo_object_construct (BONOBO_OBJECT (service), corba_service)) {
		g_warning ("bonobo_object_construct failed");
	}	
}

/*
  The  GtkType generator. Again, see GGAD for more 
 */
GtkType
trilobite_eazel_time_service_get_type() {
	static GtkType trilobite_service_type = 0;

	/* g_message ("into trilobite_eazel_time_service_get_type");  */

	/* First time it's called ? */
	if (!trilobite_service_type)
	{
		static const GtkTypeInfo trilobite_service_info =
		{
			"TrilobiteEazelTimeService",
			sizeof (TrilobiteService),
			sizeof (TrilobiteServiceClass),
			(GtkClassInitFunc) trilobite_eazel_time_service_class_initialize,
			(GtkObjectInitFunc) trilobite_eazel_time_service_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		/* Get a unique GtkType */
		trilobite_service_type = gtk_type_unique (bonobo_object_get_type (), &trilobite_service_info);
	}

	return trilobite_service_type;
}

/*
  The _new method simply builds the service
  using gtk_object_new
*/
TrilobiteEazelTimeService*
trilobite_eazel_time_service_new()
{
	TrilobiteEazelTimeService *service;

	/* g_message ("in trilobite_eazel_time_service_new"); */
	
	service = TRILOBITE_EAZEL_TIME_SERVICE (gtk_object_new (TRILOBITE_TYPE_EAZEL_TIME_SERVICE, NULL));
	
	return service;
}

/* 
   NOTE: 
   the parser isn't using XML, since the contents of body is so basic 
 */
static time_t
trilobite_eazel_time_service_parse_body (char *body) 
{
	time_t result;
	char *ptr, *nptr;

	result = 0;
	ptr = strstr (body, "<time>");

	if (ptr!= NULL) {
		ptr += strlen ("<time>");
		result = strtol (ptr, &nptr, 10);
		if (nptr == NULL || (strncmp (nptr, "</time>",7) != 0) ) {
			result = 0;
		}
	}
	
	return result;
}

/*
  Requesting via. HTTP
*/
static time_t
trilobite_eazel_time_service_do_http_request (TrilobiteEazelTimeService *service,
					      CORBA_Environment *ev) 
{
	time_t result;
	ghttp_request *request;
	ghttp_status request_status;

	g_return_val_if_fail (service != NULL, 0);
	g_return_val_if_fail (TRILOBITE_IS_EAZEL_TIME_SERVICE (service), 0);
	g_return_val_if_fail (service->private != NULL, 0);

	request = ghttp_request_new ();
	ghttp_set_header (request, http_hdr_User_Agent, "Trilobite");
	ghttp_set_uri (request, service->private->time_url);
	ghttp_set_type (request, ghttp_type_get);
	ghttp_prepare (request);
	request_status = ghttp_process (request);

	switch (request_status) {
	case ghttp_error: {
		Trilobite_Eazel_Time_CannotGetTime *exn; 
		const char *reason;
		exn = Trilobite_Eazel_Time_CannotGetTime__alloc ();			
		
		reason = ghttp_get_error (request);
		g_message ("setting exn \"%s\" \"%s\"", service->private->time_url, reason);
		exn->url = CORBA_string_dup (service->private->time_url);
		exn->reason = reason==NULL ? CORBA_string_dup ("Bad url"): CORBA_string_dup ( reason );
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_Trilobite_Eazel_Time_CannotGetTime, exn);
		
		result = 0;
	}
	break;
	case ghttp_not_done:
		g_message ("hest");
		result = 0;
		break;
	case ghttp_done:
		result = trilobite_eazel_time_service_parse_body (ghttp_get_body (request));
		break;
	}

	ghttp_clean (request);
	g_free (request);

	return result;
}

time_t
trilobite_eazel_time_service_get_server_time (TrilobiteEazelTimeService *service,
					      CORBA_Environment *ev) 
{
	time_t result;
	time_t now;

	now = time (NULL);
	if ((service->private->time_obtained > 0) &&
	    (now - service->private->time_obtained < TIME_SERVER_CACHE_TIMEOUT)) {
		/* don't bug the time server again -- just extrapolate the time */
		return service->private->server_time + (now - service->private->time_obtained);
	}

	switch (service->private->method) {
	case REQUEST_BY_HTTP:
		result = trilobite_eazel_time_service_do_http_request (service, ev);
		break;
	default:
		result = 0;
		break;
	};

	service->private->server_time = result;
	if (result != 0) {
		service->private->time_obtained = time (NULL);
	}

	return result;
}
