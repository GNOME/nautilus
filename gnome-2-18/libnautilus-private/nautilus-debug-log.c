/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-debug-log.c: Ring buffer for logging debug messages

   Copyright (C) 2006 Novell, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Federico Mena-Quintero <federico@novell.com>
*/
#include <config.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <eel/eel-glib-extensions.h>
#include "nautilus-debug-log.h"
#include "nautilus-file.h"

#define DEFAULT_RING_BUFFER_NUM_LINES 1000

#define KEY_FILE_GROUP		"debug log"
#define KEY_FILE_DOMAINS_KEY	"enable domains"
#define KEY_FILE_MAX_LINES_KEY	"max lines"

static GStaticMutex log_mutex = G_STATIC_MUTEX_INIT;

static GHashTable *domains_hash;
static char **ring_buffer;
static int ring_buffer_next_index;
static int ring_buffer_num_lines;
static int ring_buffer_max_lines = DEFAULT_RING_BUFFER_NUM_LINES;

static GSList *milestones_head;
static GSList *milestones_tail;

static void
lock (void)
{
	g_static_mutex_lock (&log_mutex);
}

static void
unlock (void)
{
	g_static_mutex_unlock (&log_mutex);
}

void
nautilus_debug_log (gboolean is_milestone, const char *domain, const char *format, ...)
{
	va_list args;

	va_start (args, format);
	nautilus_debug_logv (is_milestone, domain, NULL, format, args);
	va_end (args);
}

static gboolean
is_domain_enabled (const char *domain)
{
	/* User actions are always logged */
	if (strcmp (domain, NAUTILUS_DEBUG_LOG_DOMAIN_USER) == 0)
		return TRUE;

	if (!domains_hash)
		return FALSE;

	return (g_hash_table_lookup (domains_hash, domain) != NULL);
}

static void
ensure_ring (void)
{
	if (ring_buffer)
		return;

	ring_buffer = g_new0 (char *, ring_buffer_max_lines);
	ring_buffer_next_index = 0;
	ring_buffer_num_lines = 0;
}

static void
add_to_ring (char *str)
{
	ensure_ring ();

	g_assert (str != NULL);

	if (ring_buffer_num_lines == ring_buffer_max_lines) {
		/* We have an overlap, and the ring_buffer_next_index points to
		 * the "first" item.  Free it to make room for the new item.
		 */

		g_assert (ring_buffer[ring_buffer_next_index] != NULL);
		g_free (ring_buffer[ring_buffer_next_index]);
	} else
		ring_buffer_num_lines++;

	g_assert (ring_buffer_num_lines <= ring_buffer_max_lines);

	ring_buffer[ring_buffer_next_index] = str;

	ring_buffer_next_index++;
	if (ring_buffer_next_index == ring_buffer_max_lines) {
		ring_buffer_next_index = 0;
		g_assert (ring_buffer_num_lines == ring_buffer_max_lines);
	}
}

static void
add_to_milestones (const char *str)
{
	char *str_copy;

	str_copy = g_strdup (str);

	if (milestones_tail) {
		milestones_tail = g_slist_append (milestones_tail, str_copy);
		milestones_tail = milestones_tail->next;
	} else {
		milestones_head = milestones_tail = g_slist_append (NULL, str_copy);
	}

	g_assert (milestones_head != NULL && milestones_tail != NULL);
}

void
nautilus_debug_logv (gboolean is_milestone, const char *domain, const GList *uris, const char *format, va_list args)
{
	char *str;
	char *debug_str;
	struct timeval tv;
	struct tm tm;

	lock ();

	if (!(is_milestone || is_domain_enabled (domain)))
		goto out;

	str = g_strdup_vprintf (format, args);
	gettimeofday (&tv, NULL);

	tm = *localtime (&tv.tv_sec);

	debug_str = g_strdup_printf ("%p %04d/%02d/%02d %02d:%02d:%02d.%04d (%s): %s",
				     g_thread_self (),
				     tm.tm_year + 1900,
				     tm.tm_mon + 1,
				     tm.tm_mday,
				     tm.tm_hour,
				     tm.tm_min,
				     tm.tm_sec,
				     (int) (tv.tv_usec / 100),
				     domain,
				     str);
	g_free (str);

	if (uris) {
		int debug_str_len;
		int uris_len;
		const GList *l;
		char *new_str;
		char *p;

		uris_len = 0;

		for (l = uris; l; l = l->next) {
			const char *uri;

			uri = l->data;
			uris_len += strlen (uri) + 2; /* plus 2 for a tab and the newline */
		}

		debug_str_len = strlen (debug_str);
		new_str = g_new (char, debug_str_len + 1 + uris_len); /* plus 1 for newline */

		p = g_stpcpy (new_str, debug_str);
		*p++ = '\n';

		for (l = uris; l; l = l->next) {
			const char *uri;

			uri = l->data;

			*p++ = '\t';

			p = g_stpcpy (p, uri);

			if (l->next)
				*p++ = '\n';
		}

		g_free (debug_str);
		debug_str = new_str;
	}

	add_to_ring (debug_str);
	if (is_milestone)
		add_to_milestones (debug_str);

 out:
	unlock ();
}

void
nautilus_debug_log_with_uri_list (gboolean is_milestone, const char *domain, const GList *uris,
				  const char *format, ...)
{
	va_list args;

	va_start (args, format);
	nautilus_debug_logv (is_milestone, domain, uris, format, args);
	va_end (args);
}

void
nautilus_debug_log_with_file_list (gboolean is_milestone, const char *domain, GList *files,
				   const char *format, ...)
{
	va_list args;
	GList *uris;
	GList *l;

	uris = NULL;

	for (l = files; l; l = l->next) {
		NautilusFile *file;
		char *uri;

		file = NAUTILUS_FILE (l->data);
		uri = nautilus_file_get_uri (file);

		if (nautilus_file_is_gone (file)) {
			char *new_uri;

			/* Hack: this will create an invalid URI, but it's for
			 * display purposes only.
			 */
			new_uri = g_strconcat (uri ? uri : "", " (gone)", NULL);
			g_free (uri);
			uri = new_uri;
		}
		uris = g_list_prepend (uris, uri);
	}

	uris = g_list_reverse (uris);

	va_start (args, format);
	nautilus_debug_logv (is_milestone, domain, uris, format, args);
	va_end (args);

	eel_g_list_free_deep (uris);
}

gboolean
nautilus_debug_log_load_configuration (const char *filename, GError **error)
{
	GKeyFile *key_file;
	char **strings;
	gsize num_strings;
	int num;
	GError *my_error;

	g_assert (filename != NULL);
	g_assert (error == NULL || *error == NULL);

	key_file = g_key_file_new ();

	if (!g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, error)) {
		g_key_file_free (key_file);
		return FALSE;
	}

	/* Domains */

	my_error = NULL;
	strings = g_key_file_get_string_list (key_file, KEY_FILE_GROUP, KEY_FILE_DOMAINS_KEY, &num_strings, &my_error);
	if (my_error)
		g_error_free (my_error);
	else {
		int i;

		for (i = 0; i < num_strings; i++)
			strings[i] = g_strstrip (strings[i]);

		nautilus_debug_log_enable_domains ((const char **) strings, num_strings);
		g_strfreev (strings);
	}

	/* Number of lines */

	my_error = NULL;
	num = g_key_file_get_integer (key_file, KEY_FILE_GROUP, KEY_FILE_MAX_LINES_KEY, &my_error);
	if (my_error)
		g_error_free (my_error);
	else
		nautilus_debug_log_set_max_lines (num);

	g_key_file_free (key_file);
	return TRUE;
}

void
nautilus_debug_log_enable_domains (const char **domains, int n_domains)
{
	int i;

	g_assert (domains != NULL);
	g_assert (n_domains >= 0);

	lock ();

	if (!domains_hash)
		domains_hash = g_hash_table_new (g_str_hash, g_str_equal);

	for (i = 0; i < n_domains; i++) {
		g_assert (domains[i] != NULL);

		if (strcmp (domains[i], NAUTILUS_DEBUG_LOG_DOMAIN_USER) == 0)
			continue; /* user actions are always enabled */

		if (g_hash_table_lookup (domains_hash, domains[i]) == NULL) {
			char *domain;

			domain = g_strdup (domains[i]);
			g_hash_table_insert (domains_hash, domain, domain);
		}
	}

	unlock ();
}

void
nautilus_debug_log_disable_domains (const char **domains, int n_domains)
{
	int i;

	g_assert (domains != NULL);
	g_assert (n_domains >= 0);

	lock ();

	if (domains_hash) {
		for (i = 0; i < n_domains; i++) {
			char *domain;

			g_assert (domains[i] != NULL);

			if (strcmp (domains[i], NAUTILUS_DEBUG_LOG_DOMAIN_USER) == 0)
				continue; /* user actions are always enabled */

			domain = g_hash_table_lookup (domains_hash, domains[i]);
			if (domain) {
				g_hash_table_remove (domains_hash, domain);
				g_free (domain);
			}
		}
	} /* else, there is nothing to disable */

	unlock ();
}

gboolean
nautilus_debug_log_is_domain_enabled (const char *domain)
{
	gboolean retval;

	g_assert (domain != NULL);

	lock ();
	retval = is_domain_enabled (domain);
	unlock ();

	return retval;
}

struct domains_dump_closure {
	char **domains;
	int num_domains;
};

static void
domains_foreach_dump_cb (gpointer key, gpointer value, gpointer data)
{
	struct domains_dump_closure *closure;
	char *domain;

	closure = data;
	domain = key;

	closure->domains[closure->num_domains] = domain;
	closure->num_domains++;
}

static GKeyFile *
make_key_file_from_configuration (void)
{
	GKeyFile *key_file;
	struct domains_dump_closure closure;
	int num_domains;

	key_file = g_key_file_new ();

	/* domains */

	if (domains_hash) {
		num_domains = g_hash_table_size (domains_hash);
		if (num_domains != 0) {
			closure.domains = g_new (char *, num_domains);
			closure.num_domains = 0;

			g_hash_table_foreach (domains_hash, domains_foreach_dump_cb, &closure);
			g_assert (num_domains == closure.num_domains);

			g_key_file_set_string_list (key_file, KEY_FILE_GROUP, KEY_FILE_DOMAINS_KEY,
						    (const gchar * const *) closure.domains, closure.num_domains);
			g_free (closure.domains);
		}
	}

	/* max lines */

	g_key_file_set_integer (key_file, KEY_FILE_GROUP, KEY_FILE_MAX_LINES_KEY, ring_buffer_max_lines);

	return key_file;
}

static gboolean
write_string (const char *filename, FILE *file, const char *str, GError **error)
{
	if (fputs (str, file) == EOF) {
		int saved_errno;

		saved_errno = errno;
		g_set_error (error,
			     G_FILE_ERROR,
			     g_file_error_from_errno (saved_errno),
			     "error when writing to log file %s", filename);

		return FALSE;
	}

	return TRUE;
}

static gboolean
dump_configuration (const char *filename, FILE *file, GError **error)
{
	GKeyFile *key_file;
	char *data;
	gsize length;
	gboolean success;

	if (!write_string (filename, file,
			   "\n\n"
			   "This configuration for the debug log can be re-created\n"
			   "by putting the following in ~/nautilus-debug-log.conf\n"
			   "(use ';' to separate domain names):\n\n",
			   error)) {
		return FALSE;
	}

	success = FALSE;

	key_file = make_key_file_from_configuration ();

	data = g_key_file_to_data (key_file, &length, error);
	if (!data)
		goto out;

	if (!write_string (filename, file, data, error)) {
		goto out;
	}

	success = TRUE;
 out:
	g_key_file_free (key_file);
	return success;
}

static gboolean
dump_milestones (const char *filename, FILE *file, GError **error)
{
	GSList *l;

	if (!write_string (filename, file, "===== BEGIN MILESTONES =====\n", error))
		return FALSE;

	for (l = milestones_head; l; l = l->next) {
		const char *str;

		str = l->data;
		if (!(write_string (filename, file, str, error)
		      && write_string (filename, file, "\n", error)))
			return FALSE;
	}

	if (!write_string (filename, file, "===== END MILESTONES =====\n", error))
		return FALSE;

	return TRUE;
}

static gboolean
dump_ring_buffer (const char *filename, FILE *file, GError **error)
{
	int start_index;
	int i;

	if (!write_string (filename, file, "===== BEGIN RING BUFFER =====\n", error))
		return FALSE;

	if (ring_buffer_num_lines == ring_buffer_max_lines)
		start_index = ring_buffer_next_index;
	else
		start_index = 0;

	for (i = 0; i < ring_buffer_num_lines; i++) {
		int idx;

		idx = (start_index + i) % ring_buffer_max_lines;

		if (!(write_string (filename, file, ring_buffer[idx], error)
		      && write_string (filename, file, "\n", error))) {
			return FALSE;
		}
	}

	if (!write_string (filename, file, "===== END RING BUFFER =====\n", error))
		return FALSE;

	return TRUE;
}

gboolean
nautilus_debug_log_dump (const char *filename, GError **error)
{
	FILE *file;
	gboolean success;

	g_assert (error == NULL || *error == NULL);

	lock ();

	success = FALSE;

	file = fopen (filename, "w");
	if (!file) {
		int saved_errno;

		saved_errno = errno;
		g_set_error (error,
			     G_FILE_ERROR,
			     g_file_error_from_errno (saved_errno),
			     "could not open log file %s", filename);
		goto out;
	}

	if (!(dump_milestones (filename, file, error)
	      && dump_ring_buffer (filename, file, error)
	      && dump_configuration (filename, file, error))) {
		goto do_close;
	}

	success = TRUE;

 do_close:

	if (fclose (file) != 0) {
		int saved_errno;

		saved_errno = errno;

		if (error && *error) {
			g_error_free (*error);
			*error = NULL;
		}

		g_set_error (error,
			     G_FILE_ERROR,
			     g_file_error_from_errno (saved_errno),
			     "error when closing log file %s", filename);
		success = FALSE;
	}

 out:

	unlock ();
	return success;
}

void
nautilus_debug_log_set_max_lines (int num_lines)
{
	char **new_buffer;
	int lines_to_copy;

	g_assert (num_lines > 0);

	lock ();

	if (num_lines == ring_buffer_max_lines)
		goto out;

	new_buffer = g_new0 (char *, num_lines);

	lines_to_copy = MIN (num_lines, ring_buffer_num_lines);

	if (ring_buffer) {
		int start_index;
		int i;

		if (ring_buffer_num_lines == ring_buffer_max_lines)
			start_index = (ring_buffer_next_index + ring_buffer_max_lines - lines_to_copy) % ring_buffer_max_lines;
		else
			start_index = ring_buffer_num_lines - lines_to_copy;

		g_assert (start_index >= 0 && start_index < ring_buffer_max_lines);

		for (i = 0; i < lines_to_copy; i++) {
			int idx;

			idx = (start_index + i) % ring_buffer_max_lines;

			new_buffer[i] = ring_buffer[idx];
			ring_buffer[idx] = NULL;
		}

		for (i = 0; i < ring_buffer_max_lines; i++)
			g_free (ring_buffer[i]);

		g_free (ring_buffer);
	}

	ring_buffer = new_buffer;
	ring_buffer_next_index = lines_to_copy;
	ring_buffer_num_lines = lines_to_copy;
	ring_buffer_max_lines = num_lines;

 out:

	unlock ();
}

int
nautilus_debug_log_get_max_lines (void)
{
	int retval;

	lock ();
	retval = ring_buffer_max_lines;
	unlock ();

	return retval;
}

void
nautilus_debug_log_clear (void)
{
	int i;

	lock ();

	if (!ring_buffer)
		goto out;

	for (i = 0; i < ring_buffer_max_lines; i++) {
		g_free (ring_buffer[i]);
		ring_buffer[i] = NULL;
	}

	ring_buffer_next_index = 0;
	ring_buffer_num_lines = 0;

 out:
	unlock ();
}
