/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

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

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include "help-method.h"
#include <libgnomevfs/gnome-vfs-module.h>
#include <libgnomevfs/gnome-vfs-module-shared.h>

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


static gboolean file_in_info_path (const char *file);


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
	gchar *retval;
	gchar *after_method;
	gchar *escaped_uri;

	retval = NULL;
	after_method = NULL;
	escaped_uri = NULL;
	
	switch (help_uri->type) {
	case SGML_FILE:
		if (help_uri->section) {
			after_method = g_strdup_printf ("gnome-db2html2 %s?%s;mime-type=text/html",
						  help_uri->file, help_uri->section);
			escaped_uri = gnome_vfs_escape_string (after_method);
			retval = g_strconcat ("pipe:",  escaped_uri, NULL);		
                } else {
			after_method = g_strdup_printf ("gnome-db2html2 %s;mime-type=text/html",
						  help_uri->file);
			escaped_uri = gnome_vfs_escape_string (after_method);
			retval = g_strconcat ("pipe:", escaped_uri, NULL);
                }
		break;
	case MAN_FILE:
		after_method = g_strdup_printf ("gnome-man2html2 %s;mime-type=text/html",
					  help_uri->file);
		escaped_uri = gnome_vfs_escape_string (after_method);
		retval = g_strconcat ("pipe:", escaped_uri, NULL);
		break;
	case INFO_FILE:
		after_method = g_strdup_printf ("gnome-info2html2 %s;mime-type=text/html",
					  help_uri->file);
		escaped_uri = gnome_vfs_escape_string (after_method);
		retval = g_strconcat ("pipe:", escaped_uri, NULL);
		break;
	case HTML_FILE:
		if (help_uri->section) {
			retval = g_strdup_printf ("file://%s#%s", help_uri->file, help_uri->section);
		} else {
			retval = g_strdup_printf ("file://%s", help_uri->file);
                }
		break;
	default:
		/* FIXME: This needs to be removed so things can be handled more
		 * gracefully (i.e. g_warning) */
		g_assert_not_reached ();
	}
	if (after_method != NULL) {
		g_free (after_method);
	}
	if (escaped_uri != NULL) {
		g_free (escaped_uri);
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
	
	if (!g_file_test (file, G_FILE_TEST_ISFILE | G_FILE_TEST_ISLINK)) { 
		return FALSE;
	}

	help_uri->file = file;
	mime_type = gnome_vfs_get_file_mime_type (file, NULL, FALSE);
		
	if (strcmp (mime_type, "text/sgml") == 0) {
		help_uri->type = SGML_FILE;
	} else if (strcmp (mime_type, "text/html") == 0) {
		help_uri->type = HTML_FILE;
	} else if (strcmp (mime_type, "application/x-troff-man") == 0) {
		help_uri->type = MAN_FILE;
	} else if (file_in_info_path (file)) {
  	        help_uri->type = INFO_FILE;
	}

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
	if (p == NULL) {
		p = strrchr (g_strchomp((gchar *)file), '#');
	}

	if (p) {
		help_uri->section = g_strdup (p+1);
		temp_file_base = g_strndup (file, p - file);
	} else {
		/* we do not want trailing spaces or it can screw things up */
		temp_file_base = g_strdup (g_strchomp ((gchar *) file));
	}

	/* Concantation to TOPHELPDIR commented out because this is an
	 * ABSOLUTE uri
	temp_file = g_concat_dir_and_file (TOPHELPDIR, temp_file_base); */
	/* First we try the file directly */
	temp_file = g_strdup (temp_file_base);
	
	if (convert_file_to_uri (help_uri, temp_file)) {
		g_free (temp_file_base);
		return help_uri;
	}

	/* Next, we try to add extensions */
	temp_file2 = g_strdup_printf ("%s.sgml", temp_file);
	if (convert_file_to_uri (help_uri, temp_file2) != FALSE) {
		g_free (temp_file);
		g_free (temp_file_base);
		return help_uri;
	}

	g_free (temp_file2);
	temp_file2 = g_strdup_printf ("%s.html", temp_file);
	if (convert_file_to_uri (help_uri, temp_file2) != FALSE) {
		g_free (temp_file);
		g_free (temp_file_base);
		return help_uri;
	}
	
	return NULL;
}

/* Possible cases for 'path' in this function are:
 * path/to/file[.sgml]
 * file[.sgml]
 */

static char *
file_from_path (const char *path) {
	const char *slash, *period;
	char *retval;

	/* Get rid of the path to just get the filename */
	retval = NULL;
	period = NULL;
	slash = strrchr (path, '/');
	
	if (slash != NULL) {
		period = strchr (slash, '.');
	} else {
		period = strchr (path, '.');
	}
	if (period != NULL) {
		if (slash == NULL) {
			/* e.g. file.sgml */
			retval = g_strndup (path, period - path);
		} else {
			/* e.g. path/to/file.sgml */
			slash = slash + 1; /* Get rid of leading '/' */
			retval = g_strndup (slash, period - slash);
		}
	} else {
		if (slash != NULL) {
			/* e.g. path/to/file */
			retval = g_strndup (slash+1, strlen (slash+1));
		} else {
			/* e.g. file */
			retval = g_strdup (path);
		}
	}
	
	return retval;
}

static HelpURI *
transform_relative_file (const gchar *file)
{
	HelpURI *help_uri;
	gchar *temp_file, *temp_file_base, *temp_file2;
	gchar *appname;
	gchar *p;

	help_uri = help_uri_new ();
	
	p = strrchr (file, '?');
	if (p == NULL) {
		p = strrchr (g_strchomp ((gchar *)file), '#');
	}

	if (p) {
		help_uri->section = g_strdup (p+1);
		temp_file_base = g_strndup (file, p - file);
	} else {
		temp_file_base = g_strdup (g_strchomp ((gchar *) file));
	}
	
	appname = file_from_path (temp_file_base);
	
	if (strcmp (appname, "") == 0) {
		/* NULL string */
		g_free (appname);
		appname = NULL;
	}
	
	if (appname == NULL)
		return NULL;

	/* Get the help file while taking i18n into account */
	temp_file = gnome_help_file_path (appname, temp_file_base);
	
	if (convert_file_to_uri (help_uri, temp_file) != FALSE) {
		g_free (temp_file_base);
		return help_uri;
	}

	/* Try to add some extensions */
	temp_file2 = g_strdup_printf ("%s.sgml", temp_file);
	if (convert_file_to_uri (help_uri, temp_file2) != FALSE) {
		g_free (temp_file);
		g_free (temp_file_base);
		return help_uri;
	}

	g_free (temp_file2);
	temp_file2 = g_strdup_printf ("%s.html", temp_file);
	if (convert_file_to_uri (help_uri, temp_file2) != FALSE) {
		g_free (temp_file);
		g_free (temp_file_base);
		return help_uri;
	}
	
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
		return GNOME_VFS_ERROR_NOT_FOUND;

	if (old_uri[0] == '/') {
		help_uri = transform_absolute_file (old_uri);
	} else {
		help_uri = transform_relative_file (old_uri);
	}

	if (help_uri == NULL) {
		return GNOME_VFS_ERROR_NOT_FOUND;
	}

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


static gboolean
file_in_info_path (const char *file) 
{
        const char *info_path;
        char **info_path_strv;
        int i;

        /* Check some hardcoded locations. */
        if (strncmp (file, "/usr/info/", strlen ("/usr/info/")) == 0 ||
            strncmp (file, "/usr/local/info/", strlen ("/usr/local/info/")) ||
            strncmp (file, "/usr/local/info/", strlen ("/usr/gnome/info/"))) {
                return TRUE;
        }

        /* Check the INFOPATH */
        info_path = getenv ("INFOPATH");
        if (info_path != NULL) {
                info_path_strv = g_strsplit (info_path, ":", 0);

                for (i = 0; info_path_strv [i] != NULL; i++) {
                        if (strncmp (file, info_path_strv[i], strlen (info_path_strv[i])) == 0) {
                                g_strfreev (info_path_strv);
                                return TRUE;
                        }
                }

                g_strfreev (info_path_strv);
        }

        return FALSE;
}



