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

#include <config.h>
#include "help-method.h"

#include <ctype.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-module-shared.h>
#include <libgnomevfs/gnome-vfs-module.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-method.h>
#include <stdio.h>
#include <string.h>

static gboolean already_initialized = FALSE;
G_LOCK_DEFINE_STATIC (already_initialized);

static GHashTable *app_list = NULL;
G_LOCK_DEFINE_STATIC (app_list);

typedef enum {
	SGML_FILE,
	MAN_FILE,
	INFO_FILE,
	HTML_FILE,
	XML_FILE, 
	UNKNOWN_FILE
} HelpFileTypes;

typedef struct {
	char *file; /* The absolute path */
	char *section;
	HelpFileTypes type;
} HelpURI;


static gboolean file_in_info_path (const char *file);

/* This is a copy of eel_shell_quote. The best thing to do is to
 * use the one on GNOME for GNOME 2.0, but we could also move this
 * into gnome-vfs or some other library so we could share it.
 */
static char *
shell_quote (const char *string)
{
	const char *p;
	GString *quoted_string;
	char *quoted_str;

	/* All kinds of ways to do this fancier.
	 * - Detect when quotes aren't needed at all.
	 * - Use double quotes when they would look nicer.
	 * - Avoid sequences of quote/unquote in a row (like when you quote "'''").
	 * - Do it higher speed with strchr.
	 * - Allocate the GString with g_string_sized_new.
	 */

	g_return_val_if_fail (string != NULL, NULL);

	quoted_string = g_string_new ("'");

	for (p = string; *p != '\0'; p++) {
		if (*p == '\'') {
			/* Get out of quotes, do a quote, then back in. */
			g_string_append (quoted_string, "'\\''");
		} else {
			g_string_append_c (quoted_string, *p);
		}
	}

	g_string_append_c (quoted_string, '\'');

	/* Let go of the GString. */
	quoted_str = quoted_string->str;
	g_string_free (quoted_string, FALSE);

	return quoted_str;
}

static HelpURI *
help_uri_new (void)
{
	HelpURI *retval;

	retval = g_new0 (HelpURI, 1);
	retval->type = UNKNOWN_FILE;

	return retval;
}

static char *
help_uri_to_string (HelpURI *help_uri)
{
        const char *command;
	char *parameter, *command_line, *escaped, *uri;

	switch (help_uri->type) {
	case SGML_FILE: case XML_FILE:
#ifdef USE_GNOME_DB2HTML3
		command = "gnome-db2html3";
#else
                command = "gnome-db2html2";
#endif
		if (help_uri->section != NULL) {
                        parameter = g_strconcat (help_uri->file, "?", help_uri->section, NULL);
                } else {
			parameter = g_strdup (help_uri->file);
                }
		break;
	case MAN_FILE:
                command = "gnome-man2html2";
                parameter = g_strdup (help_uri->file);
		break;
	case INFO_FILE:
                command = "gnome-info2html2";
                parameter = g_strdup (help_uri->file);
		break;
	case HTML_FILE:
                escaped = gnome_vfs_escape_path_string (help_uri->file);
		if (help_uri->section == NULL) {
                        uri = g_strconcat ("file://", escaped, NULL);
                } else {
                        uri = g_strconcat ("file://", escaped, "#", help_uri->section, NULL);
                }
                g_free (escaped);
		return uri;
	case UNKNOWN_FILE:
		return NULL;
	default:
		/* FIXME bugzilla.gnome.org 42401: 
		 * An assert at runtime may be a bit harsh.
                 * We'd prefer behavior more like g_return_if_fail.
                 * In glib 2.0 we can use g_return_val_if_reached.
                 */
		g_assert_not_reached ();
                return NULL;
	}

        if (parameter[0] == '-') {
                g_free (parameter);
                return NULL;
        }
        
        /* Build a command line. */
        escaped = shell_quote (parameter);
        g_free (parameter);
        command_line = g_strconcat (command, " ", escaped, ";mime-type=text/html", NULL);
        g_free (escaped);
        escaped = gnome_vfs_escape_string (command_line);
        g_free (command_line);
        uri = g_strconcat ("pipe:", escaped, NULL);
        g_free (escaped);

	return uri;
}

static void
help_uri_free (HelpURI *help_uri)
{
	g_free (help_uri->file);
	g_free (help_uri->section);
	g_free (help_uri);
}

static void
init_help_module (void)
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
	G_UNLOCK (app_list);
}

static gboolean
convert_file_to_uri (HelpURI *help_uri, char *file)
{
	const char *mime_type;

	/* FIXME: This test is no longer necessary since we know the file exists from calling function */
	if (!g_file_test (file, G_FILE_TEST_ISFILE | G_FILE_TEST_ISLINK)) { 
		return FALSE;
	}

	help_uri->file = file;
	mime_type = gnome_vfs_get_file_mime_type (file, NULL, FALSE);
        
	if (g_strcasecmp (mime_type, "text/sgml") == 0) {
		help_uri->type = SGML_FILE;
	} else if (g_strcasecmp (mime_type, "text/xml") == 0) {
		help_uri->type = XML_FILE;
	} else if (g_strcasecmp (mime_type, "text/html") == 0) {
		help_uri->type = HTML_FILE;
	} else if (g_strcasecmp (mime_type, "application/x-troff-man") == 0) {
                /* FIXME bugzilla.gnome.org 42402: 
                 * The check above used to check for a prefix
                 * of "application/x-troff-man", but now we check for
                 * an exact string match. Is that what we really want?
                 */
		help_uri->type = MAN_FILE;
	} else if (file_in_info_path (file)) {
  	        help_uri->type = INFO_FILE;
	} else {
		help_uri->type = UNKNOWN_FILE;
	}

	return TRUE;
}

static gboolean
string_ends_in (const char *string, const char *suffix)
{
	size_t string_len, suffix_len;

	string_len = strlen (string);
	suffix_len = strlen (suffix);

	if (suffix_len > string_len) {
		return FALSE;
	} else {
		return 0 == strcmp (string + strlen (string) - strlen (suffix), suffix);
	}
}

static char *
strdup_string_to_substring_end (const char *string, const char *substring)
{
	const char *marker;
	size_t substring_length;

	if (string == NULL || substring == NULL) {
		return NULL;
	}
	
	substring_length = strlen (substring);
	marker = strstr (string, substring);

	if (marker == NULL) {
		return NULL;
	}

	marker += substring_length;

	return g_strndup (string, marker-string);
}

/*
 * bugzilla.gnome.org 46761:
 * Automatically promote requests for html help to sgml help
 * if available
 *
 * ghelp:/.../gfoo/C/index.html     -> ghelp:/.../gfoo/C/gfoo.sgml#index
 * ghelp:/.../gfoo/C/index.html#abc -> ghelp:/.../gfoo/C/gfoo.sgml#abc
 * ghelp:/.../gfoo/C/stuff.html     -> ghelp:/.../gfoo/C/gfoo.sgml#stuff
 * ghelp:/.../gfoo/C/stuff.html#def -> ghelp:/.../gfoo/C/gfoo.sgml#def
 */
static void
check_sgml_promotion (const char *base, /*OUT*/ char **p_new_uri, /*INOUT*/ char **p_section)
{
	gchar **path_split;
	char *help_dir_base;
	char *sgml_path;

	g_return_if_fail (p_new_uri != NULL);
	g_return_if_fail (p_section != NULL);

	if (!string_ends_in (base, ".html")) {
		*p_new_uri = g_strdup (base);
		return;
	}

	/*
	 * The path format is assumed to be
	 * share/gnome/help/ <application> / <locale> / <resource>
	 * Note that the fragment has already been stripped and is passed in
	 * separately
	 */

	help_dir_base = strdup_string_to_substring_end (base, "share/gnome/help/");

	if (help_dir_base == NULL) {
		*p_new_uri = g_strdup (base);
		return;
	}
	
	path_split = g_strsplit (base + strlen(help_dir_base), "/" , 3);

	if (path_split[0] == NULL || path_split[1] == NULL || path_split[2] == NULL
	    || strchr (path_split[2], '/') != NULL ) {
		g_strfreev (path_split);
		*p_new_uri = g_strdup (base);
		return;
	}

	/* sgml document name should be "application.sgml" */
	sgml_path = g_strconcat (help_dir_base, path_split[0], "/", path_split[1], "/", path_split[0], ".sgml", NULL);

	if (g_file_exists (sgml_path)) {
		*p_new_uri = sgml_path;
		sgml_path = NULL;

		/* resources not equal to index.html turn into sections
		 * if there is no section already defined
		 * (presense of ".html" suffix was asserted above)
		 */
		if (0 != strcmp (path_split[2], "index.html") && *p_section == NULL) {
			/* chew off .html */

			path_split[2][strlen (path_split[2]) - strlen (".html")] = '\0';
			g_free (*p_section);
			*p_section = g_strdup (path_split[2]);
		}
	} else {
		*p_new_uri = g_strdup (base);
	}
}
static char *
file_from_path (const char *path)
{
	const char *slash, *period;

	/* Get rid of the path to just get the filename */
	slash = strrchr (path, '/');
	if (slash != NULL) {
		period = strchr (slash, '.');
	} else {
		period = strchr (path, '.');
	}

	if (period != NULL) {
		if (slash == NULL) {
			/* e.g. file.sgml */
			return g_strndup (path, period - path);
		} else {
			/* e.g. path/to/file.sgml */
			slash = slash + 1; /* Get rid of leading '/' */
			return g_strndup (slash, period - slash);
		}
	} else {
		if (slash != NULL) {
			/* e.g. path/to/file */
			return g_strdup (slash+1);
		} else {
			/* e.g. file */
			return g_strdup (path);
		}
	}
}

/* If the help file exists returns the appropriate PATH (taking locale into account)
 * otherwise it returns NULL */
static char *
help_name_to_local_path (const char *old_uri)
{
        char *base_name, *new_uri, *buf;
	GList *language_list;
	char *new_uri_with_extension;
	char *old_help;
        gboolean is_toc;

        is_toc = FALSE;
        
        base_name = file_from_path (old_uri);
        if (base_name == NULL || base_name[0] == '\0') {
                g_free (base_name);
                return NULL;
        }

        is_toc = strcmp (old_uri, "toc") == 0;
        
	new_uri_with_extension = NULL;
	new_uri = NULL;
        
	language_list = gnome_i18n_get_language_list ("LC_MESSAGES");

	while (!new_uri_with_extension && language_list) {
		const char *lang;

		lang = language_list->data;
                if (is_toc)
                        buf = g_strdup_printf ("gnome/help/help-browser/%s/default-page.html",
                                               lang);
                else
                        buf = g_strdup_printf ("gnome/help/%s/%s/%s", base_name, lang, old_uri);

		new_uri = gnome_unconditional_datadir_file (buf);
		g_free (buf);

                if (is_toc) {
                        if (g_file_exists (new_uri)) {
                                new_uri_with_extension = new_uri;
                                new_uri = NULL;
                        }
                } else {
                        new_uri_with_extension = g_strconcat (new_uri, ".xml", NULL);
                        /* FIXME: Should we use g_file_test instead? */
                        if (!g_file_exists (new_uri_with_extension)) {
                                /* XML file doesn't exist - now try SGML */
                                g_free (new_uri_with_extension);
                                
                                new_uri_with_extension = g_strconcat (new_uri, ".sgml", NULL);
                                if (!g_file_exists (new_uri_with_extension)) {
				/* SGML file doesn't exist - fallback to SGML */
                                        g_free (new_uri_with_extension);
                                        
                                        old_help = g_strdup_printf ("gnome/help/%s/%s/index.html", base_name, lang);
                                        new_uri_with_extension = gnome_unconditional_datadir_file (old_help);
                                        g_free (old_help);
                                        
                                        if (!g_file_exists (new_uri_with_extension)) {
                                                /* HTML file doesn't exist - next language */
                                                g_free (new_uri_with_extension);
                                                new_uri_with_extension = NULL;
                                        }
                                }
                        }
                }

                g_free (new_uri);
                new_uri = NULL;
                language_list = language_list->next;
        }
        
	return new_uri_with_extension;			
}

/* We can handle sgml, info and html files only.
 *
 * Possible cases for absolute paths:
 * 
 * /path/to/file[.sgml][?section]
 * /path/to/file[.html][#section]
 * /absolute/path/to/file[.sgml]
 *
 * Possible cases for relative paths:
 * path/to/file[.sgml]
 * file[.sgml]
 */
 
static HelpURI *
transform_file (const char *old_uri)
{
	HelpURI *help_uri;
	char *p;
	char *base, *new_uri;

	help_uri = help_uri_new ();

        /* Find the part after either a "?" or a "#". Only look for a
         * "#" if there is no "?". (We could instead use strpbrk to
         * search for the first occurence of either "?" or "#".) 
         */
	p = strrchr (old_uri, '?');
	if (p == NULL) {
		p = strrchr (old_uri, '#');
        }

	if (p == NULL) {
		base = g_strdup (old_uri);
	} else {
		help_uri->section = g_strdup (p + 1);
		base = g_strndup (old_uri, p - old_uri);
	}

        /* We do not want trailing spaces or it can screw things up. */
        g_strchomp (base);

	if (base != NULL && base[0] == '/') {
		/* If an html file is specifed but an sgml file is present
		 * we want to use that instead
		 */
		check_sgml_promotion (base, &new_uri, &(help_uri->section));
	} else { 	
		new_uri = help_name_to_local_path (base);
	}

        g_free (base);
	
        if (new_uri == NULL) {
		/* there is no SGML/XML or old HTML help path */
                help_uri_free (help_uri);
                return NULL;
        }

        /* Try the URI. */
	if (convert_file_to_uri (help_uri, new_uri)) {
		return help_uri;
	}

        /* Failed, so return. */
	g_free (new_uri);
        help_uri_free (help_uri);
	return NULL;
}

static GnomeVFSResult
help_do_transform (GnomeVFSTransform *transform,
		   const char *old_uri,
		   char **new_uri,
		   GnomeVFSContext *context)
{
	HelpURI *help_uri;

	*new_uri = NULL;
	if (old_uri == NULL || *old_uri == '\0') {
		return GNOME_VFS_ERROR_NOT_FOUND;
        }

	help_uri = transform_file (old_uri);
	
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
            strncmp (file, "/usr/gnome/info/", strlen ("/usr/gnome/info/")) ||
	    strncmp (file, "/usr/share/info/", strlen ("/usr/share/info/"))) {
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

