/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-services-support.c - Functions for using services from Nautilus.

   Copyright (C) 2001 Eazel, Inc.

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

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "nautilus-services.h"

#include <libnautilus-extensions/nautilus-mime-actions.h>
#ifdef HAVE_AMMONITE
#include <libtrilobite/libammonite.h>
#include <bonobo/bonobo-main.h>
#endif

/* FIXME bugzilla.eazel.com xxxx: 
 *
 * Perhaps this needs to be a more generic scheme ?
 */
#define NAUTILUS_SERVICES_URI_SCHEME "eazel"
#define NAUTILUS_SERVICES_PROTOCOL "eazel-services://"

/* Make an OAF query to determine whether a component that can handle services
 * exists.  For now, the query is done only once.  If we want to have services
 * be available without having to restart Nautilus, we would have to change this
 */
gboolean
nautilus_services_are_enabled (void)
{
	static gboolean enabled = FALSE;
	static gboolean enabled_known = FALSE;

	if (!enabled_known) {
		enabled = nautilus_mime_has_any_components_for_uri_scheme (NAUTILUS_SERVICES_URI_SCHEME);
		enabled_known = TRUE;
	}

	return enabled;
}

char *
nautilus_services_get_summary_uri (void)
{
	return g_strdup (NAUTILUS_SERVICES_URI_SCHEME ":");
}

char *
nautilus_services_get_user_name (void)
{
#ifdef HAVE_AMMONITE
	if (ammonite_init (bonobo_poa ())) {
		return ammonite_get_default_user_username ();
	}
#endif

	return NULL;
}

char *
nautilus_services_get_online_storage_uri (void)
{
	char *user_name;
	char *uri;

	user_name = nautilus_services_get_user_name ();

	/* FIXME bugzilla.eazel.com 5036: user feedback needs to be displayed in this case */
	/* Something better than just going to the summary page */
	if (user_name == NULL) {
		return nautilus_services_get_summary_uri ();
	}
	
	uri = g_strdup_printf ("%s/~%s", NAUTILUS_SERVICES_PROTOCOL, user_name);
	g_free (user_name);

	return uri;
}

char *
nautilus_services_get_software_catalog_uri (void)
{
	char *user_name;
	char *uri;

	user_name = nautilus_services_get_user_name ();

	uri = user_name != NULL ?
		g_strdup ("eazel-services:///catalog") :
		g_strdup ("eazel-services://anonymous/catalog");

	g_free (user_name);

	return uri;
}
