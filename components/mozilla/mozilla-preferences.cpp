/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Ramiro Estrugo <ramiro@eazel.com>
 *
 */

/*
 * mozilla-preferences.cpp - A small C wrapper for poking mozilla preferences
 */

#include <config.h>

#include "mozilla-preferences.h"

#include <libgnome/gnome-defs.h>
#include <gtk/gtkobject.h>
#include <gtk/gtkwidget.h>
#include <gconf/gconf-client.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>

#include <stdlib.h>

#include "nsIServiceManager.h"
#include "nsIPref.h"

#define nopeDEBUG_mfleming 1

#ifdef DEBUG_mfleming
#define DEBUG_MSG(x)	g_print x
#else
#define DEBUG_MSG(x)	
#endif

static GConfClient *preferences_get_global_gconf_client       (void);
static void         preferences_proxy_sync_mozilla_with_gconf (void);

static const char PROXY_HOST_KEY[] = "/system/gnome-vfs/http-proxy-host";
static const char PROXY_PORT_KEY[] = "/system/gnome-vfs/http-proxy-port";
static const char USE_PROXY_KEY[] = "/system/gnome-vfs/use-http-proxy";
static const char SYSTEM_GNOME_VFS_PATH[] = "/system/gnome-vfs";

extern "C" gboolean
mozilla_preference_set (const char *preference_name,
			const char *new_value)
{
	g_return_val_if_fail (preference_name != NULL, FALSE);
	g_return_val_if_fail (new_value != NULL, FALSE);

	nsCOMPtr<nsIPref> pref = do_CreateInstance (NS_PREF_CONTRACTID);
	
	if (pref)
	{
		nsresult rv = pref->SetCharPref (preference_name, new_value);

		return NS_SUCCEEDED (rv) ? TRUE : FALSE;
	}

	return FALSE;
}

extern "C" gboolean
mozilla_preference_set_boolean (const char *preference_name,
				gboolean new_boolean_value)
{
	g_return_val_if_fail (preference_name != NULL, FALSE);

	nsCOMPtr<nsIPref> pref = do_CreateInstance (NS_PREF_CONTRACTID);
	
	if (pref)
	{
		nsresult rv = pref->SetBoolPref (preference_name,
						 new_boolean_value ? PR_TRUE : PR_FALSE);
		
		return NS_SUCCEEDED (rv) ? TRUE : FALSE;
	}

	return FALSE;
}

extern "C" gboolean
mozilla_preference_set_int (const char *preference_name,
			    gint new_int_value)
{
	g_return_val_if_fail (preference_name != NULL, FALSE);

	nsCOMPtr<nsIPref> pref = do_CreateInstance (NS_PREF_CONTRACTID);
	
	if (pref)
	{
		nsresult rv = pref->SetIntPref (preference_name, new_int_value);
		
		return NS_SUCCEEDED (rv) ? TRUE : FALSE;
	}

	return FALSE;
}

extern "C"  gboolean
mozilla_gconf_handle_gconf_error (GError **error)
{
	static gboolean shown_dialog = FALSE;
	
	g_return_val_if_fail (error != NULL, FALSE);

	if (*error != NULL) {
		g_warning (_("GConf error:\n  %s"), (*error)->message);
		if (!shown_dialog) {
			char *message;
			GtkWidget *dialog;
			
			shown_dialog = TRUE;

			message = g_strdup_printf (_("GConf error:\n  %s\n"
						     "All further errors shown "
						     "only on terminal"),
						   (*error)->message);
			
			dialog = gnome_error_dialog (message);
		}
		g_error_free (*error);
		*error = NULL;
		
		return TRUE;
	}
	
	return FALSE;
}

static guint gconf_proxy_host_changed_connection = 0;
static guint gconf_proxy_port_changed_connection = 0;
static guint gconf_use_http_proxy_changed_connection = 0;

static void
preferences_proxy_changed_callback (GConfClient* client,
				    guint cnxn_id,
				    GConfEntry *entry,
				    gpointer user_data)
{
	preferences_proxy_sync_mozilla_with_gconf ();
}

static void
preferences_add_gconf_proxy_connections (void)
{
	GConfClient *gconf_client;
	GError *error = NULL;

	gconf_client = preferences_get_global_gconf_client ();
	g_return_if_fail (GCONF_IS_CLIENT (gconf_client));
	
	gconf_proxy_host_changed_connection = gconf_client_notify_add (gconf_client,
								       PROXY_HOST_KEY,
								       preferences_proxy_changed_callback,
								       NULL,
								       NULL,
								       &error);
	mozilla_gconf_handle_gconf_error (&error);

	gconf_proxy_port_changed_connection = gconf_client_notify_add (gconf_client,
								       PROXY_PORT_KEY,
								       preferences_proxy_changed_callback,
								       NULL,
								       NULL,
								       &error);
	mozilla_gconf_handle_gconf_error (&error);

	gconf_use_http_proxy_changed_connection = gconf_client_notify_add (gconf_client,
									   USE_PROXY_KEY,
									   preferences_proxy_changed_callback,
									   NULL,
									   NULL,
									   &error);
	mozilla_gconf_handle_gconf_error (&error);
}

static void
preferences_remove_gconf_proxy_connections (void)
{
	GConfClient *gconf_client;

	gconf_client = preferences_get_global_gconf_client ();
	g_return_if_fail (GCONF_IS_CLIENT (gconf_client));
	
	gconf_client_notify_remove (gconf_client, gconf_proxy_host_changed_connection);
	gconf_proxy_host_changed_connection = 0;

	gconf_client_notify_remove (gconf_client, gconf_proxy_port_changed_connection);
	gconf_proxy_port_changed_connection = 0;

	gconf_client_notify_remove (gconf_client, gconf_use_http_proxy_changed_connection);
	gconf_use_http_proxy_changed_connection = 0;
}

extern "C" void
mozilla_gconf_listen_for_proxy_changes (void)
{
	static gboolean once = FALSE;
	GConfClient *gconf_client;
	GError *error = NULL;
	
	if (once == TRUE) {
		return;
	}

	once = TRUE;

	/* Sync the first time */
	preferences_proxy_sync_mozilla_with_gconf ();

	gconf_client = preferences_get_global_gconf_client ();

	g_return_if_fail (GCONF_IS_CLIENT (gconf_client));
	
	/* Let gconf know about ~/.gconf/system/gnome-vfs */
	gconf_client_add_dir (gconf_client,
			      SYSTEM_GNOME_VFS_PATH,
			      GCONF_CLIENT_PRELOAD_NONE,
			      &error);

	mozilla_gconf_handle_gconf_error (&error);
	
	preferences_add_gconf_proxy_connections ();

	g_atexit (preferences_remove_gconf_proxy_connections);
}

static GConfClient *global_gconf_client = NULL;

static void
preferences_unref_global_gconf_client (void)
{
	if (global_gconf_client == NULL) {
		gtk_object_unref (GTK_OBJECT (global_gconf_client));
	}

	global_gconf_client = NULL;
}

/* Get the default gconf client.  Initialize gconf if needed. */
static GConfClient *
preferences_get_global_gconf_client (void)
{
	/* Initialize gconf if needed */
	if (!gconf_is_initialized ()) {
		GError *error = NULL;
		char *argv[] = { "nautilus-mozilla-component", NULL };
		
		if (!gconf_init (1, argv, &error)) {
			
			if (mozilla_gconf_handle_gconf_error (&error)) {
				return NULL;
			}
		}
	}

	if (global_gconf_client == NULL) {
		global_gconf_client = gconf_client_get_default ();
		g_atexit (preferences_unref_global_gconf_client);
	}

	return global_gconf_client;
}

/* Setup http proxy prefrecens.  This is done by using gconf to read the 
 * /system/gnome-vfs/proxy preference and then seting this information
 * for the mozilla networking library via mozilla preferences. 
 */
static void
preferences_proxy_sync_mozilla_with_gconf (void)
{
	GConfClient *gconf_client;
	char *proxy_string = NULL;
	GError *error = NULL;
	
	gconf_client = preferences_get_global_gconf_client ();

        g_return_if_fail (GCONF_IS_CLIENT (gconf_client));
	
	if (gconf_client_get_bool (gconf_client, USE_PROXY_KEY, &error)) {
		char *proxy_host = NULL;
		gboolean proxy_host_success;
		int proxy_port = 8080;
		gboolean proxy_port_success;

		error = NULL;
		proxy_host = gconf_client_get_string (gconf_client, PROXY_HOST_KEY, &error);
		proxy_host_success = !mozilla_gconf_handle_gconf_error (&error);

		error = NULL;
		proxy_port = gconf_client_get_int (gconf_client, PROXY_PORT_KEY, &error);
		proxy_port_success = !mozilla_gconf_handle_gconf_error (&error);

		if (proxy_host_success) {
			mozilla_preference_set ("network.proxy.http", proxy_host);

			if (proxy_port_success && proxy_port != 0) {
				DEBUG_MSG(("mozilla-view: setting http proxy to %s:%u\n", proxy_host, (unsigned)proxy_port));
				mozilla_preference_set_int ("network.proxy.http_port", proxy_port);
			} else {
				DEBUG_MSG(("mozilla-view: setting http proxy to %s:%u\n", proxy_host, (unsigned)8080));
				mozilla_preference_set_int ("network.proxy.http_port", 8080);
			}
			
			/* 1, Configure proxy settings manually */
			mozilla_preference_set_int ("network.proxy.type", 1);
		}

		g_free (proxy_host);

		return;
	}

	DEBUG_MSG(("mozilla-view: disabling HTTP proxy\n"));

	/* Default is 0, which conects to internet hosts directly */
	mozilla_preference_set_int ("network.proxy.type", 0);
}
