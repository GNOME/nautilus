/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Nautilus
 *
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
 */

#include <config.h>

#include "nautilus-desktop-metadata.h"

#include "nautilus-directory-notify.h"
#include "nautilus-file-private.h"
#include "nautilus-file-utilities.h"

#include <glib/gstdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static guint save_in_idle_source_id = 0;

static gchar *
get_keyfile_path (void)
{
	gchar *xdg_dir, *retval;

	xdg_dir = nautilus_get_user_directory ();
	retval = g_build_filename (xdg_dir, "desktop-metadata", NULL);

	g_free (xdg_dir);
	
	return retval;
}

static gboolean
save_in_idle_cb (gpointer data)
{
	GKeyFile *keyfile = data;
	gchar *contents, *filename;
	gsize length;
	GError *error = NULL;

	save_in_idle_source_id = 0;

	contents = g_key_file_to_data (keyfile, &length, NULL);
	filename = get_keyfile_path ();

	if (contents != NULL) {
		g_file_set_contents (filename,
				     contents, length,
				     &error);
	}

	if (error != NULL) {
		g_warning ("Couldn't save the desktop metadata keyfile to disk: %s",
			   error->message);
		g_error_free (error);
	}

	return FALSE;
}

static void
save_in_idle (GKeyFile *keyfile)
{
	if (save_in_idle_source_id != 0) {
		g_source_remove (save_in_idle_source_id);
	}

	save_in_idle_source_id = g_idle_add (save_in_idle_cb, keyfile);
}

static GKeyFile *
load_metadata_keyfile (void)
{
  	GKeyFile *retval;
	GError *error = NULL;
	gchar *filename;

	retval = g_key_file_new ();
	filename = get_keyfile_path ();

	g_key_file_load_from_file (retval,
				   filename,
				   G_KEY_FILE_NONE,
				   &error);

	if (error != NULL) {
		g_print ("Unable to open the desktop metadata keyfile: %s\n",
			 error->message);

		g_error_free (error);
	}

	g_free (filename);

	return retval;
}

static GKeyFile *
get_keyfile (void)
{
	static gboolean keyfile_loaded = FALSE;
	static GKeyFile *keyfile = NULL;

	if (!keyfile_loaded) {
		keyfile = load_metadata_keyfile ();
		keyfile_loaded = TRUE;
	}

	return keyfile;
}

void
nautilus_desktop_set_metadata_string (NautilusFile *file,
                                      const gchar *name,
                                      const gchar *key,
                                      const gchar *string)
{
	GKeyFile *keyfile;

	keyfile = get_keyfile ();

	g_key_file_set_string (keyfile,
			       name,
			       key,
			       string);

	save_in_idle (keyfile);

	if (nautilus_desktop_update_metadata_from_keyfile (file, name)) {
		nautilus_file_changed (file);
	}	
}

void
nautilus_desktop_set_metadata_stringv (NautilusFile *file,
                                       const char *name,
                                       const char *key,
                                       const char * const *stringv)
{
	GKeyFile *keyfile;

	g_print ("setting desktop metadata\n");

	keyfile = get_keyfile ();

	g_key_file_set_string_list (keyfile,
				    name,
				    key,
				    stringv,
				    g_strv_length ((gchar **) stringv));

	save_in_idle (keyfile);

	if (nautilus_desktop_update_metadata_from_keyfile (file, name)) {
		nautilus_file_changed (file);
	}
}

gboolean
nautilus_desktop_update_metadata_from_keyfile (NautilusFile *file,
					       const gchar *name)
{
	gchar **keys, **values;
	const gchar *key;
	gchar *gio_key;
	gsize length, values_length;
	GKeyFile *keyfile;
	GFileInfo *info;
	gint idx;
	gboolean res;

	keyfile = get_keyfile ();

	keys = g_key_file_get_keys (keyfile,
				    name,
				    &length,
				    NULL);

	if (keys == NULL) {
		return FALSE;
	}

	info = g_file_info_new ();

	for (idx = 0; idx < length; idx++) {
		key = keys[idx];
		values = g_key_file_get_string_list (keyfile,
						     name,
						     key,
						     &values_length,
						     NULL);

		gio_key = g_strconcat ("metadata::", key, NULL);

		if (values_length < 1) {
			continue;
		} else if (values_length == 1) {
			g_file_info_set_attribute_string (info,
							  gio_key,
							  values[0]);
		} else {
			g_file_info_set_attribute_stringv (info,
							   gio_key,
							   values);
		}

		g_free (gio_key);
		g_strfreev (values);
	}

	res = nautilus_file_update_metadata_from_info (file, info);

	g_strfreev (keys);
	g_object_unref (info);

	return res;
}
