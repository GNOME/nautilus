/*
 * Copyright (C) 2000 Red Hat Inc.
 * All rights reserved.
 *
 * This module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/* FIXME: This prolly doesn't handle escaping correctly.  This needs some thought.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include <gnome.h>

#include "gnome-vfs.h"
#include "help-method.h"
#include "gnome-vfs-module.h"
#include "module-shared.h"

/* FIXME: temporary var, until we get i18n involved */
#define TOPHELPDIR "/usr/share/gnome/help2/"

static gboolean already_initialized = FALSE;
G_LOCK_DEFINE_STATIC (already_initialized);

static GHashTable *app_list = NULL;
G_LOCK_DEFINE_STATIC (app_list);

typedef enum {
	SGML_FILE,
	MAN_FILE,
	INFO_FILE,
	HTML_FILE,
	UNKNOWN_FILE
} HelpFileTypes;

typedef struct {
	gchar *file; /* The absolute path */
	gchar *section;
	HelpFileTypes type;
} HelpURI;

static HelpURI *
help_uri_new ()
{
	HelpURI *retval;

	retval = g_new0 (HelpURI, 1);
	retval->type = UNKNOWN_FILE;

	return retval;
}

static gchar *
help_uri_to_string (HelpURI *help_uri)
{
	gchar *retval = NULL;
	switch (help_uri->type) {
	case SGML_FILE:
		if (help_uri->section)
			retval = g_strdup_printf ("pipe:gnome-db2html2 %s?%s",
						  help_uri->file, help_uri->section);
		else
			retval = g_strdup_printf ("pipe:gnome-db2html2 %s",
						  help_uri->file);
		break;
	case MAN_FILE:
		retval = g_strdup_printf ("pipe:gnome-man2html2 %s",
					  help_uri->file);
		break;
	case INFO_FILE:
		retval = g_strdup_printf ("pipe:gnome-info2html2 %s",
					  help_uri->file);
		break;
	case HTML_FILE:
		if (help_uri->section)
			retval = g_strdup_printf ("file://%s#%s", help_uri->file, help_uri->section);
		else
			retval = g_strdup_printf ("file://%s", help_uri->file);
		break;
	default:
		g_assert_not_reached ();
	}

	return retval;
}

static void
help_uri_free (HelpURI *help_uri)
{
	g_free (help_uri->file);
	g_free (help_uri->section);
	g_free (help_uri);
}

static void
init_help_module ()
{
	G_LOCK (already_initialized);
	if (already_initialized) {
		G_UNLOCK (already_initialized);
		return;
	}
	already_initialized = TRUE;
	G_UNLOCK (already_initialized);

	G_LOCK (app_list);
	app_list = g_hash_table_new (g_str_hash, g_str_equal);
	G_UNLOCK (already_initialized);
}

static gboolean
convert_file_to_uri (HelpURI *help_uri, gchar *file)
{
	const gchar *mime_type;

	if (!g_file_test (file, G_FILE_TEST_ISFILE | G_FILE_TEST_ISLINK))
		return FALSE;

	help_uri->file = file;
	mime_type = gnome_mime_type_of_file (file);
	if (!strcmp (mime_type, "text/sgml") ||
	    !strcmp (mime_type, "exported SGML document text"))
		help_uri->type = SGML_FILE;
	else if (!strcmp (mime_type, "text/html"))
		help_uri->type = HTML_FILE;
	else if (!strncmp (mime_type, "application/x-troff-man", strlen ("application/x-troff-man")))
		help_uri->type = MAN_FILE;
	/* FIXME: test for info pages! */

	return TRUE;
}	

/* We can handle sgml, info and html files only.
 *
 * possible formats:
 * 
 * /path/to/file[.sgml][?section]
 * /path/to/file[.html][#section]
 * /absolute/path/to/file[.sgml]
 */
static HelpURI *
transform_absolute_file (const gchar *file)
{
	HelpURI *help_uri;
	gchar *temp_file_base, *temp_file2, *temp_file;
	gchar *p;

	help_uri = help_uri_new ();

	p = strrchr (file, '?');
	if (p == NULL)
		p = strrchr (file, '#');

	if (p) {
		help_uri->section = g_strdup (p+1);
		temp_file_base = g_strndup (file, p - file);
	} else {
		temp_file_base = g_strdup (file);
	}

	/* First we try the file directly */
	/* FIXME: we need to deal with locale, too */
	temp_file = g_concat_dir_and_file (TOPHELPDIR, temp_file_base);
	if (convert_file_to_uri (help_uri, temp_file)) {
		g_free (temp_file_base);
		return help_uri;
	}

	/* Next, we try to add extensions */
	temp_file2 = g_strdup_printf ("%s.sgml", temp_file);
	if (convert_file_to_uri (help_uri, temp_file)) {
		g_free (temp_file);
		g_free (temp_file_base);
		return help_uri;
	}

	g_free (temp_file2);
	temp_file2 = g_strdup_printf ("%s.html", temp_file);
	if (convert_file_to_uri (help_uri, temp_file)) {
		g_free (temp_file);
		g_free (temp_file_base);
		return help_uri;
	}
	return NULL;
}

static HelpURI *
transform_relative_file (const gchar *file)
{
	return NULL;
}

static GnomeVFSResult
help_do_transform (GnomeVFSTransform *transform,
		   const gchar *old_uri,
		   gchar **new_uri,
		   GnomeVFSContext *context)
{
	HelpURI *help_uri;

	*new_uri = NULL;

	if (old_uri == NULL || *old_uri == '\000')
		return GNOME_VFS_ERROR_NOTFOUND;

	if (old_uri[0] == '/')
		help_uri = transform_absolute_file (old_uri);
	else
		help_uri = transform_relative_file (old_uri);

	if (help_uri == NULL)
		return GNOME_VFS_ERROR_NOTFOUND;

	*new_uri = help_uri_to_string (help_uri);
	help_uri_free (help_uri);
	return GNOME_VFS_OK;
}

static GnomeVFSTransform transform = {
	help_do_transform
};

GnomeVFSTransform *
vfs_module_transform (const char *method_name, const char *args)
{
	init_help_module ();
	return &transform;
}
