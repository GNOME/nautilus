/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* gnome-vfs-utils.c - Utility gnome-vfs methods. Will use gnome-vfs
                       HEAD in time.

   Copyright (C) 1999 Free Software Foundation
   Copyright (C) 2000, 2001 Eazel, Inc.

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

   Authors: Ettore Perazzoli <ettore@comm2000.it>
   	    John Sullivan <sullivan@eazel.com> 
            Darin Adler <darin@eazel.com>
*/

#include <config.h>

#include "egg-recent-vfs-utils.h"

#include <libgnomevfs/gnome-vfs-utils.h>
#include <string.h>
#include <stdlib.h>

#ifdef ENABLE_NLS
#include <glib.h>

#include <libintl.h>
#define _(String) gettext(String)

#ifdef gettext_noop
#define N_(String) gettext_noop(String)
#else
#define N_(String) (String)
#endif
#else /* NLS is disabled */
#define _(String) (String)
#define N_(String) (String)
#define textdomain(String) (String)
#define gettext(String) (String)
#define dgettext(Domain,String) (String)
#define dcgettext(Domain,String,Type) (String)
#define bindtextdomain(Domain,Directory) (Domain)
#endif

static char *
make_valid_utf8 (const char *name)
{
	GString *string;
	const char *remainder, *invalid;
	int remaining_bytes, valid_bytes;

	string = NULL;
	remainder = name;
	remaining_bytes = strlen (name);

	while (remaining_bytes != 0) {
		if (g_utf8_validate (remainder, remaining_bytes, &invalid)) {
			break;
		}
		valid_bytes = invalid - remainder;

		if (string == NULL) {
			string = g_string_sized_new (remaining_bytes);
		}
		g_string_append_len (string, remainder, valid_bytes);
		g_string_append_c (string, '?');

		remaining_bytes -= valid_bytes + 1;
		remainder = invalid + 1;
	}

	if (string == NULL) {
		return g_strdup (name);
	}

	g_string_append (string, remainder);
	g_string_append (string, _(" (invalid Unicode)"));
	g_assert (g_utf8_validate (string->str, -1, NULL));

	return g_string_free (string, FALSE);
}

static gboolean
istr_has_prefix (const char *haystack, const char *needle)
{
	const char *h, *n;
	char hc, nc;

	/* Eat one character at a time. */
	h = haystack == NULL ? "" : haystack;
	n = needle == NULL ? "" : needle;
	do {
		if (*n == '\0') {
			return TRUE;
		}
		if (*h == '\0') {
			return FALSE;
		}
		hc = *h++;
		nc = *n++;
		hc = g_ascii_tolower (hc);
		nc = g_ascii_tolower (nc);
	} while (hc == nc);
	return FALSE;
}

static gboolean
str_has_prefix (const char *haystack, const char *needle)
{
	const char *h, *n;

	/* Eat one character at a time. */
	h = haystack == NULL ? "" : haystack;
	n = needle == NULL ? "" : needle;
	do {
		if (*n == '\0') {
			return TRUE;
		}
		if (*h == '\0') {
			return FALSE;
		}
	} while (*h++ == *n++);
 	return FALSE;
}

static gboolean
uri_is_local_scheme (const char *uri)
{
	gboolean is_local_scheme;
	char *temp_scheme;
	int i;
	char *local_schemes[] = {"file:", "help:", "ghelp:", "gnome-help:",
				 "trash:", "man:", "info:", 
				 "hardware:", "search:", "pipe:",
				 "gnome-trash:", NULL};

	is_local_scheme = FALSE;
	for (temp_scheme = *local_schemes, i = 0; temp_scheme != NULL; i++, temp_scheme = local_schemes[i]) {
		is_local_scheme = istr_has_prefix (uri, temp_scheme);
		if (is_local_scheme) {
			break;
		}
	}

	return is_local_scheme;
}

static char *
handle_trailing_slashes (const char *uri)
{
	char *temp, *uri_copy;
	gboolean previous_char_is_column, previous_chars_are_slashes_without_column;
	gboolean previous_chars_are_slashes_with_column;
	gboolean is_local_scheme;

	g_assert (uri != NULL);

	uri_copy = g_strdup (uri);
	if (strlen (uri_copy) <= 2) {
		return uri_copy;
	}

	is_local_scheme = uri_is_local_scheme (uri);

	previous_char_is_column = FALSE;
	previous_chars_are_slashes_without_column = FALSE;
	previous_chars_are_slashes_with_column = FALSE;

	/* remove multiple trailing slashes */
	for (temp = uri_copy; *temp != '\0'; temp++) {
		if (*temp == '/' && !previous_char_is_column) {
			previous_chars_are_slashes_without_column = TRUE;
		} else if (*temp == '/' && previous_char_is_column) {
			previous_chars_are_slashes_without_column = FALSE;
			previous_char_is_column = TRUE;
			previous_chars_are_slashes_with_column = TRUE;
		} else {
			previous_chars_are_slashes_without_column = FALSE;
			previous_char_is_column = FALSE;
			previous_chars_are_slashes_with_column = FALSE;
		}

		if (*temp == ':') {
			previous_char_is_column = TRUE;
		}
	}

	if (*temp == '\0' && previous_chars_are_slashes_without_column) {
		if (is_local_scheme) {
			/* go back till you remove them all. */
			for (temp--; *(temp) == '/'; temp--) {
				*temp = '\0';
			}
		} else {
			/* go back till you remove them all but one. */
			for (temp--; *(temp - 1) == '/'; temp--) {
				*temp = '\0';
			}			
		}
	}

	if (*temp == '\0' && previous_chars_are_slashes_with_column) {
		/* go back till you remove them all but three. */
		for (temp--; *(temp - 3) != ':' && *(temp - 2) != ':' && *(temp - 1) != ':'; temp--) {
			*temp = '\0';
		}
	}


	return uri_copy;
}

static char *
make_uri_canonical (const char *uri)
{
	char *canonical_uri, *old_uri, *p;
	gboolean relative_uri;

	relative_uri = FALSE;

	if (uri == NULL) {
		return NULL;
	}

	/* FIXME bugzilla.eazel.com 648: 
	 * This currently ignores the issue of two uris that are not identical but point
	 * to the same data except for the specific cases of trailing '/' characters,
	 * file:/ and file:///, and "lack of file:".
	 */

	canonical_uri = handle_trailing_slashes (uri);

	/* Note: In some cases, a trailing slash means nothing, and can
	 * be considered equivalent to no trailing slash. But this is
	 * not true in every case; specifically not for web addresses passed
	 * to a web-browser. So we don't have the trailing-slash-equivalence
	 * logic here, but we do use that logic in EelDirectory where
	 * the rules are more strict.
	 */

	/* Add file: if there is no scheme. */
	if (strchr (canonical_uri, ':') == NULL) {
		old_uri = canonical_uri;

		if (old_uri[0] != '/') {
			/* FIXME bugzilla.eazel.com 5069: 
			 *  bandaid alert. Is this really the right thing to do?
			 * 
			 * We got what really is a relative path. We do a little bit of
			 * a stretch here and assume it was meant to be a cryptic absolute path,
			 * and convert it to one. Since we can't call gnome_vfs_uri_new and
			 * gnome_vfs_uri_to_string to do the right make-canonical conversion,
			 * we have to do it ourselves.
			 */
			relative_uri = TRUE;
			canonical_uri = gnome_vfs_make_path_name_canonical (old_uri);
			g_free (old_uri);
			old_uri = canonical_uri;
			canonical_uri = g_strconcat ("file:///", old_uri, NULL);
		} else {
			canonical_uri = g_strconcat ("file:", old_uri, NULL);
		}
		g_free (old_uri);
	}

	/* Lower-case the scheme. */
	for (p = canonical_uri; *p != ':'; p++) {
		g_assert (*p != '\0');
		*p = g_ascii_tolower (*p);
	}

	if (!relative_uri) {
		old_uri = canonical_uri;
		canonical_uri = gnome_vfs_make_uri_canonical (canonical_uri);
		if (canonical_uri != NULL) {
			g_free (old_uri);
		} else {
			canonical_uri = old_uri;
		}
	}
	
	/* FIXME bugzilla.eazel.com 2802:
	 * Work around gnome-vfs's desire to convert file:foo into file://foo
	 * by converting to file:///foo here. When you remove this, check that
	 * typing "foo" into location bar does not crash and returns an error
	 * rather than displaying the contents of /
	 */
	if (str_has_prefix (canonical_uri, "file://")
	    && !str_has_prefix (canonical_uri, "file:///")) {
		old_uri = canonical_uri;
		canonical_uri = g_strconcat ("file:/", old_uri + 5, NULL);
		g_free (old_uri);
	}

	return canonical_uri;
}

static char *
format_uri_for_display (const char *uri, gboolean filenames_are_locale_encoded)
{
	char *canonical_uri, *path, *utf8_path;

	g_return_val_if_fail (uri != NULL, g_strdup (""));

	canonical_uri = make_uri_canonical (uri);

	/* If there's no fragment and it's a local path. */
	path = gnome_vfs_get_local_path_from_uri (canonical_uri);
	
	if (path != NULL) {
		if (filenames_are_locale_encoded) {
			utf8_path = g_locale_to_utf8 (path, -1, NULL, NULL, NULL);
			if (utf8_path) {
				g_free (canonical_uri);
				g_free (path);
				return utf8_path;
			} 
		} else if (g_utf8_validate (path, -1, NULL)) {
			g_free (canonical_uri);
			return path;
		}
	}

	if (canonical_uri && !g_utf8_validate (canonical_uri, -1, NULL)) {
		utf8_path = make_valid_utf8 (canonical_uri);
		g_free (canonical_uri);
		canonical_uri = utf8_path;
	}

	g_free (path);
	return canonical_uri;
}

char *
egg_recent_vfs_format_uri_for_display (const char *uri)
{
	static gboolean broken_filenames;
	
	broken_filenames = g_getenv ("G_BROKEN_FILENAMES") != NULL;

	return format_uri_for_display (uri, broken_filenames);
}

static gboolean
is_valid_scheme_character (char c)
{
	return g_ascii_isalnum (c) || c == '+' || c == '-' || c == '.';
}

static gboolean
has_valid_scheme (const char *uri)
{
	const char *p;

	p = uri;

	if (!is_valid_scheme_character (*p)) {
		return FALSE;
	}

	do {
		p++;
	} while (is_valid_scheme_character (*p));

	return *p == ':';
}

static char *
escape_high_chars (const guchar *string)
{
	char *result;
	const guchar *scanner;
	guchar *result_scanner;
	int escape_count;
	static const gchar hex[16] = "0123456789ABCDEF";

#define ACCEPTABLE(a) ((a)>=32 && (a)<128)
	
	escape_count = 0;

	if (string == NULL) {
		return NULL;
	}

	for (scanner = string; *scanner != '\0'; scanner++) {
		if (!ACCEPTABLE(*scanner)) {
			escape_count++;
		}
	}
	
	if (escape_count == 0) {
		return g_strdup (string);
	}

	/* allocate two extra characters for every character that
	 * needs escaping and space for a trailing zero
	 */
	result = g_malloc (scanner - string + escape_count * 2 + 1);
	for (scanner = string, result_scanner = result; *scanner != '\0'; scanner++) {
		if (!ACCEPTABLE(*scanner)) {
			*result_scanner++ = '%';
			*result_scanner++ = hex[*scanner >> 4];
			*result_scanner++ = hex[*scanner & 15];
			
		} else {
			*result_scanner++ = *scanner;
		}
	}

	*result_scanner = '\0';

	return result;
}

static char *
make_uri_from_input_internal (const char *text,
				  gboolean filenames_are_locale_encoded,
				  gboolean strip_trailing_whitespace)
{
	char *stripped, *path, *uri, *locale_path, *filesystem_path, *escaped;

	g_return_val_if_fail (text != NULL, g_strdup (""));

	/* Strip off leading whitespaces (since they can't be part of a valid
	   uri).   Only strip off trailing whitespaces when requested since
	   they might be part of a valid uri.
	 */
	if (strip_trailing_whitespace) {
		stripped = g_strstrip (g_strdup (text));
	} else {
		stripped = g_strchug (g_strdup (text));
	}

	switch (stripped[0]) {
	case '\0':
		uri = g_strdup ("");
		break;
	case '/':
		if (filenames_are_locale_encoded) {
			GError *error = NULL;
			locale_path = g_locale_from_utf8 (stripped, -1, NULL, NULL, &error);
			if (locale_path != NULL) {
				uri = gnome_vfs_get_uri_from_local_path (locale_path);
				g_free (locale_path);
			} else {
				/* We couldn't convert to the locale. */
				/* FIXME: We should probably give a user-visible error here. */
				uri = g_strdup("");
			}
		} else {
			uri = gnome_vfs_get_uri_from_local_path (stripped);
		}
		break;
	case '~':
		if (filenames_are_locale_encoded) {
			filesystem_path = g_locale_from_utf8 (stripped, -1, NULL, NULL, NULL);
		} else {
			filesystem_path = g_strdup (stripped);
		}
                /* deliberately falling into default case on fail */
		if (filesystem_path != NULL) {
			path = gnome_vfs_expand_initial_tilde (filesystem_path);
			g_free (filesystem_path);
			if (*path == '/') {
				uri = gnome_vfs_get_uri_from_local_path (path);
				g_free (path);
				break;
			}
			g_free (path);
		}
                /* don't insert break here, read above comment */
	default:
		if (has_valid_scheme (stripped)) {
			uri = escape_high_chars (stripped);
		} else {
			escaped = escape_high_chars (stripped);
			uri = g_strconcat ("http://", escaped, NULL);
			g_free (escaped);
		}
	}

	g_free (stripped);

	return uri;
	
}

char *
egg_recent_vfs_make_uri_from_input (const char *uri)
{
	static gboolean broken_filenames;

	broken_filenames = g_getenv ("G_BROKEN_FILENAMES") != NULL;

	return make_uri_from_input_internal (uri, broken_filenames, TRUE);
}

static char *
make_uri_canonical_strip_fragment (const char *uri)
{
	const char *fragment;
	char *without_fragment, *canonical;

	fragment = strchr (uri, '#');
	if (fragment == NULL) {
		return make_uri_canonical (uri);
	}

	without_fragment = g_strndup (uri, fragment - uri);
	canonical = make_uri_canonical (without_fragment);
	g_free (without_fragment);
	return canonical;
}

static gboolean
uris_match (const char *uri_1, const char *uri_2, gboolean ignore_fragments)
{
	char *canonical_1, *canonical_2;
	gboolean result;

	if (ignore_fragments) {
		canonical_1 = make_uri_canonical_strip_fragment (uri_1);
		canonical_2 = make_uri_canonical_strip_fragment (uri_2);
	} else {
		canonical_1 = make_uri_canonical (uri_1);
		canonical_2 = make_uri_canonical (uri_2);
	}

	result = strcmp (canonical_1, canonical_2) == 0;

	g_free (canonical_1);
	g_free (canonical_2);
	
	return result;
}

gboolean
egg_recent_vfs_uris_match (const char *uri_1, const char *uri_2)
{
	return uris_match (uri_1, uri_2, FALSE);
}

char *
egg_recent_vfs_get_uri_scheme (const char *uri)
{
	char *colon;

	g_return_val_if_fail (uri != NULL, NULL);

	colon = strchr (uri, ':');
	
	if (colon == NULL) {
		return NULL;
	}
	
	return g_strndup (uri, colon - uri);
}
