/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-debug.c: Nautilus debugging aids.
 
   Copyright (C) 2000 Eazel, Inc.
  
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
  
   Author: Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "nautilus-debug.h"

#include <glib.h>
#include <signal.h>
#include <stdio.h>

/* Raise a SIGINT signal to get the attention of the debugger.
   When not running under the debugger, we don't want to stop,
   so we ignore the signal for just the moment that we raise it.
*/
void
nautilus_stop_in_debugger (void)
{
	void (* saved_handler) (int);

	saved_handler = signal (SIGINT, SIG_IGN);
	raise (SIGINT);
	signal (SIGINT, saved_handler);
}

/* Stop in the debugger after running the default log handler.
   This makes certain kinds of messages stop in the debugger
   without making them fatal.
*/
static void
nautilus_stop_after_default_log_handler (const char *domain,
					 GLogLevelFlags level,
					 const char *message,
					 gpointer data)
{
	g_log_default_handler (domain, level, message, data);
	nautilus_stop_in_debugger ();
}

static void
nautilus_set_stop_after_default_log_handler (const char *domain)
{
	g_log_set_handler (domain, G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING,
			   nautilus_stop_after_default_log_handler, NULL);
}

void
nautilus_make_warnings_and_criticals_stop_in_debugger (const char *first_domain, ...)
{
	va_list domains;
	const char *domain;

	nautilus_set_stop_after_default_log_handler (first_domain);

	va_start (domains, first_domain);

	for (;;) {
		domain = va_arg (domains, const char *);
		if (domain == NULL) {
			break;
		}
		nautilus_set_stop_after_default_log_handler (domain);
	}

	va_end (domains);
}

int 
nautilus_get_available_file_descriptor_count (void)
{
	int count;
	GList *list;
	GList *p;
	FILE *file;

	list = NULL;
	for (count = 0; ; count++) {
		file = fopen("/dev/null", "r");
		if (file == NULL) {
			break;
		}
		list = g_list_prepend (list, file);
	}

	for (p = list; p != NULL; p = p->next) {
		fclose (p->data);
	}
	g_list_free (list);

	return count;
}

gboolean
nautilus_str_equal_with_free (char *eat_this,
			      const char *not_this)
{
	gboolean equal;

	/* NULL is not legal. */
	if (eat_this == NULL) {
		equal = FALSE;
	} else if (not_this == NULL) {
		equal = FALSE;
	} else {
		equal = strcmp (eat_this, not_this) == 0;
	}

	g_free (eat_this);

	return equal;
}
