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

/* FIXME bugzilla.eazel.com 698: This prolly doesn't handle escaping correctly.  This needs some thought.
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

/* #define ALI_DEBUG */

/* FIXME bugzilla.eazel.com 696: temporary var, until we get i18n involved */
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
	gchar *retval = NULL;
#ifdef ALI_DEBUG
	g_print ("help_uri->file is %s, help_uri->section is %s\n", help_uri->file, help_uri->section);
#endif
	switch (help_uri->type) {
	case SGML_FILE:
		if (help_uri->section) {
			retval = g_strdup_printf ("pipe:gnome-db2html2 %s?%s",
						  help_uri->file, help_uri->section);
                } else {
			retval = g_strdup_printf ("pipe:gnome-db2html2 %s",
						  help_uri->file);
                }
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
		if (help_uri->section) {
			retval = g_strdup_printf ("file://%s#%s", help_uri->file, help_uri->section);
		} else {
			retval = g_strdup_printf ("file://%s", help_uri->file);
                }
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
	
#ifdef ALI_DEBUG
        g_print ("file is: %s and does it exist ? %d\n",file,g_file_exists(file));
#endif
	if (!g_file_test (file, G_FILE_TEST_ISFILE | G_FILE_TEST_ISLINK)) { 
#ifdef ALI_DEBUG
		g_print ("*** g_file_test has failed\n");
#endif
		return FALSE;
	}

	help_uri->file = file;
	mime_type = gnome_vfs_mime_type_of_file (file);
#ifdef ALI_DEBUG
	g_print ("*** the file's mime_type is: %s\n",mime_type);
#endif
		
	if (strcmp (mime_type, "text/sgml") == 0 ||
	    strcmp (mime_type, "exported SGML document text") == 0) {
		help_uri->type = SGML_FILE;
	} else if (!strcmp (mime_type, "text/html")) {
		help_uri->type = HTML_FILE;
	} else if (!strncmp (mime_type, "application/x-troff-man", strlen ("application/x-troff-man"))) {
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
	if (p == NULL)
		p = strrchr (g_strchomp((gchar *)file), '#');

	if (p) {
		help_uri->section = g_strdup (p+1);
		temp_file_base = g_strndup (file, p - file);
	} else {
		/* we do not want trailing spaces or it can screw things up */
		temp_file_base = g_strdup (g_strchomp ((gchar *) file));
	}
#ifdef ALI_DEBUG
	g_print ("*** temp_file_base is: %s\n",temp_file_base);
#endif

	/* First we try the file directly */
	/* FIXME bugzilla.eazel.com 696: we need to deal with locale, too */

	/* Concaentation to TOPHELPDIR commented out because this is and
	 * ABSOLUTE uri                                                   */
	/* temp_file = g_concat_dir_and_file (TOPHELPDIR, temp_file_base); */
	temp_file = g_strdup (temp_file_base);
	
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

#ifdef ALI_DEBUG
	                g_print ("*** in help_do_transform ***\nURI is: %s\n",old_uri);
#endif

	if (old_uri == NULL || *old_uri == '\000')
		return GNOME_VFS_ERROR_NOT_FOUND;

	if (old_uri[0] == '/') {
		help_uri = transform_absolute_file (old_uri);
#ifdef ALI_DEBUG
		g_print ("*** Doing transform_absolute_file\n");
		if (help_uri != NULL) {
			g_print ("*** The return help_uri->file is: %s\n",help_uri->file);
			g_print ("*** the return help_uri->section is: %s\n",help_uri->section);
		}
		else
			g_print ("*** help_uri is NULL\n");
#endif
	}
	else
		help_uri = transform_relative_file (old_uri);

	if (help_uri == NULL)
		return GNOME_VFS_ERROR_NOT_FOUND;

	*new_uri = help_uri_to_string (help_uri);
	help_uri_free (help_uri);
#ifdef ALI_DEBUG
	g_print ("*** new_uri is %s\n", *new_uri);
#endif
	return GNOME_VFS_OK;
}

static GnomeVFSTransform transform = {
	help_do_transform
};

GnomeVFSTransform *
vfs_module_transform (const char *method_name, const char *args)
{
	init_help_module ();
#ifdef ALI_DEBUG
	g_print ("Loading libvfs-help.so...\n");
#endif
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



