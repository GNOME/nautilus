/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000 Eazel, Inc
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
 * Authors: J Shane Culpepper <pepper@eazel.com>
 *	    Robey Pointer <robey@eazel.com>
 *
 */

/*
 * libtrilobite - Useful functions shared between all services.  This
 * includes things like xml parsing, logging, error control, and others.
 *
 */

#ifndef TRILOBITE_CORE_UTILS_H
#define TRILOBITE_CORE_UTILS_H

#ifdef __GNUC__

#define trilobite_debug(format, args...) \
	g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, ##args)

#else	/* __GNUC__ */

static void
trilobite_debug (const gchar *format, ...)
{
	va_list args;
	va_start (args, format);
	g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args);
	va_end (args);
}

#endif


int trilobite_pexec (const char *path, char * const argv[], int *stdin_fd, int *stdout_fd, int *stderr_fd);
gboolean trilobite_init (const char *service_name, const char *version_name, const char *log_filename,
			 int argc, char **argv, GData *options);

#endif /* TRILOBITE_CORE_UTILS_H */

