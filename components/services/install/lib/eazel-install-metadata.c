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
 * Authors: J Shane Culpepper <pepper@eazel.com>
 *          Eskil Heyn Olsen <eskil@eazel.com>
 *          Robey Pointer <robey@eazel.com>
 */

/* eazel-install - services command line install/update/uninstall
 * component.  This program will parse the eazel-services-config.xml
 * file and install a services generated package-list.xml.
 */

#include <config.h>
#include "eazel-install-metadata.h"
#include <libtrilobite/trilobite-core-utils.h>

#ifndef EAZEL_INSTALL_SLIM
#include <gconf/gconf.h>
#include <gconf/gconf-engine.h>
static GConfEngine *conf_engine = NULL;
#endif /* EAZEL_INSTALL_SLIM */

#define INSTALL_GCONF_PATH	"/apps/eazel-trilobite/install"

/* these are NOT reasonable defaults. */
#define DEFAULT_SERVER		"services.eazel.com"
#define DEFAULT_PORT		8888
#define DEFAULT_CGI_PATH	"/catalog/find"


#ifndef EAZEL_INSTALL_SLIM
/* called by atexit so we can close the gconf connection */
static void
done_with_gconf (void)
{
	gconf_engine_unref (conf_engine);
}
#endif /* EAZEL_INSTALL_SLIM */

static void
check_gconf_init (void)
{
#ifndef EAZEL_INSTALL_SLIM
	GConfError *error = NULL;

	if (! gconf_is_initialized ()) {
		char *argv[] = { "trilobite", NULL };

		if (! gconf_init (1, argv, &error)) {
			g_assert (error != NULL);
			g_warning ("gconf init error: %s", error->str);
			gconf_error_destroy (error);
		}
	}

	if (conf_engine == NULL) {
		conf_engine = gconf_engine_get_default ();
		g_atexit (done_with_gconf);
	}
#endif /* EAZEL_INSTALL_SLIM */
}

static char *
get_conf_string (const char *key, const char *default_value)
{
#ifndef EAZEL_INSTALL_SLIM
	char *full_key;
	char *value;

	full_key = g_strdup_printf ("%s/%s", INSTALL_GCONF_PATH, key);
	value = gconf_get_string (conf_engine, full_key, NULL);
	if ((value == NULL) && (default_value != NULL)) {
		value = g_strdup (default_value);
		/* write default value to gconf */
		gconf_set_string (conf_engine, full_key, default_value, NULL);
	}
	g_free (full_key);
	return value;
#else /* EAZEL_INSTALL_SLIM */
	return g_strdup (default_value);
#endif /* EAZEL_INSTALL_SLIM */
}

static int
get_conf_int (const char *key, int default_value)
{
#ifndef EAZEL_INSTALL_SLIM
	char *full_key;
	GConfValue *value;
	int out;

	full_key = g_strdup_printf ("%s/%s", INSTALL_GCONF_PATH, key);
	value = gconf_get (conf_engine, full_key, NULL);
	if (value && (value->type == GCONF_VALUE_INT)) {
		out = gconf_value_int (value);
		gconf_value_destroy (value);
	} else {
		if (value) {
			gconf_value_destroy (value);
		}
		out = default_value;
		/* write default value to gconf */
		gconf_set_int (conf_engine, full_key, default_value, NULL);
	}

	g_free (full_key);
	return out;
#else /* EAZEL_INSTALL_SLIM */
	return default_value;
#endif /* EAZEL_INSTALL_SLIM */
}

static gboolean
get_conf_boolean (const char *key, gboolean default_value)
{
#ifndef EAZEL_INSTALL_SLIM
	char *full_key;
	GConfValue *value;
	gboolean out;

	full_key = g_strdup_printf ("%s/%s", INSTALL_GCONF_PATH, key);
	/* gconf API is so crappy that we can't use gconf_get_bool or anything nice */
	value = gconf_get (conf_engine, full_key, NULL);
	if (value && (value->type == GCONF_VALUE_BOOL)) {
		out = gconf_value_bool (value);
		gconf_value_destroy (value);
	} else {
		if (value) {
			gconf_value_destroy (value);
		}
		out = default_value;
		/* write default value to gconf */
		gconf_set_bool (conf_engine, full_key, default_value, NULL);
	}

	g_free (full_key);
	return out;
#else /* EAZEL_INSTALL_SLIM */
	return default_value;
#endif  /* EAZEL_INSTALL_SLIM */
}

static URLType
get_urltype_from_string (char* tmpbuf)
{
	URLType rv;

	if (tmpbuf[0] == 'l' || tmpbuf[0] == 'L') {
		rv = PROTOCOL_LOCAL;
	} else if (tmpbuf[0] == 'f' || tmpbuf[0] == 'F') {
		rv = PROTOCOL_FTP;
	} else if (tmpbuf[0] == 'h' || tmpbuf[0] == 'H') {
		rv = PROTOCOL_HTTP;
	} else {
		g_warning (_("Could not set URLType from config file!"));
		rv = PROTOCOL_HTTP;
	}
	return rv;
}

InstallOptions *
init_default_install_configuration (void)
{
	InstallOptions *rv;
	char *temp;

	check_gconf_init ();
	rv = g_new0 (InstallOptions, 1);

	temp = get_conf_string ("protocol", "http");
	rv->protocol = get_urltype_from_string (temp);
	g_free (temp);

	rv->pkg_list = get_conf_string ("package-list", "/var/eazel/services/package-list.xml");
	rv->transaction_dir = get_conf_string ("transaction-dir", NULL);
	if (rv->transaction_dir == NULL) {
		rv->transaction_dir = g_strdup_printf ("%s/.nautilus/transactions", g_get_home_dir ());
	}

	rv->mode_verbose = get_conf_boolean ("verbose", TRUE);
	rv->mode_silent = get_conf_boolean ("silent", FALSE);
	rv->mode_debug = get_conf_boolean ("debug", TRUE);
	rv->mode_test = get_conf_boolean ("dry-run", FALSE);
	rv->mode_force = get_conf_boolean ("force", FALSE);
	rv->mode_depend = get_conf_boolean ("depend", FALSE);
	rv->mode_update = get_conf_boolean ("allow-update", TRUE);
	rv->mode_downgrade = get_conf_boolean ("allow-downgrade", FALSE);

	return rv;
}

TransferOptions *
init_default_transfer_configuration (void)
{
	TransferOptions *rv;
	char *p;

	check_gconf_init ();
	rv = g_new0 (TransferOptions, 1);

	rv->port_number = get_conf_int ("server/port", DEFAULT_PORT);
	rv->hostname = get_conf_string ("server/hostname", DEFAULT_SERVER);
	if ((p = strchr (rv->hostname, ':')) != NULL) {
		/* make "server/port" optional -- could just be in "server/hostname" */
		*p = 0;
		rv->port_number = atoi (p+1);
	}
	rv->tmp_dir = get_conf_string ("server/temp-dir", "/tmp/eazel-install");
	rv->rpmrc_file = get_conf_string ("server/rpmrc", "/usr/lib/rpm/rpmrc");
	rv->cgi_path = get_conf_string ("server/cgi-path", DEFAULT_CGI_PATH);

	return rv;
}

void 
transferoptions_destroy (TransferOptions *topts)
{
	g_return_if_fail (topts!=NULL);

	g_free (topts->hostname);
	topts->hostname = NULL;
	g_free (topts->pkg_list_storage_path);
	topts->pkg_list_storage_path = NULL;
	g_free (topts->tmp_dir);
	topts->tmp_dir = NULL;
	g_free (topts->rpmrc_file);
	topts->rpmrc_file = NULL;
}

void 
installoptions_destroy (InstallOptions *iopts)
{
	g_return_if_fail (iopts!=NULL);

	g_free (iopts->pkg_list);
	g_free (iopts->transaction_dir);
	iopts->pkg_list = NULL;
	iopts->transaction_dir = NULL;
}
