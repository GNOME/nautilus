/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 *  Copyright (C) 2000 Eazel, Inc
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
 *  Authors: Robey Pointer <robey@eazel.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdarg.h>
#include <string.h>
#include "trilobite-core-utils.h"
#include "trilobite-core-messaging.h"

#define ROBEY_LIKES_TIMESTAMPS


static FILE *saved_logf = NULL;
static int do_debug_log = 0;
static int set_atexit = 0;


/* handler for trapping g_log/g_warning/g_error/g_message stuff, and sending it to
 * a standard logfile.
 */
static void
trilobite_add_log (const char *domain, GLogLevelFlags flags, const char *message, FILE *logf)
{
	char *prefix;
	char *timestamp = NULL;
#ifdef ROBEY_LIKES_TIMESTAMPS
	struct timeval now;
#endif

	if (logf == NULL) {
		return;
	}

	if (flags & G_LOG_LEVEL_DEBUG) {
		if (do_debug_log) {
			prefix = "d:";
		} else {
			return;
		}
	} else if (flags & G_LOG_LEVEL_MESSAGE) {
		prefix = "---";
	} else if (flags & G_LOG_LEVEL_WARNING) {
		prefix = "*** warning:";
	} else if (flags & G_LOG_LEVEL_ERROR) {
		prefix = "!!! ERROR:";
	} else {
		prefix = "???";
	}

#ifdef ROBEY_LIKES_TIMESTAMPS
	gettimeofday (&now, NULL);
	timestamp = g_malloc (40);
	strftime (timestamp, 40, "%d-%b %H:%M:%S", localtime ((time_t *)&now.tv_sec));
	sprintf (timestamp + strlen (timestamp), ".%02ld ", now.tv_usec/10000L);
#endif

	fprintf (logf, "%s%s %s\n", timestamp != NULL ? timestamp : "", prefix, message);
	fflush (logf);
}

static void
trilobite_close_log (void)
{
	if (saved_logf != NULL) {
		fclose (saved_logf);
	}
}

void
trilobite_debug (const gchar *format, ...)
{
	va_list args;
	va_start (args, format);
	g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args);
	va_end (args);
}

void
trilobite_set_debug_mode (gboolean debug_mode)
{
	do_debug_log = (debug_mode ? 1 : 0);
}

void
trilobite_set_log_handler (FILE *logf, const char *service_name)
{
	if (service_name != NULL) {
		g_log_set_handler (service_name, G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_WARNING |
				   G_LOG_LEVEL_ERROR | G_LOG_LEVEL_DEBUG,
				   (GLogFunc)trilobite_add_log, logf);
	}
	/* send libtrilobite messages there, too */
	g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_WARNING |
			   G_LOG_LEVEL_ERROR | G_LOG_LEVEL_DEBUG,
			   (GLogFunc)trilobite_add_log, logf);

	if (! set_atexit) {
		saved_logf = logf;
		g_atexit (trilobite_close_log);
		set_atexit = 1;
	}
}
