/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 */

#include <config.h>
#include "nautilus-first-time-druid.h"

#include <glib/gi18n.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <stdio.h>
#include <string.h>

void
nautilus_set_first_time_file_flag (void)
{
	FILE *stream;
	char *user_directory, *druid_flag_file_name;
	const char * const blurb =
		_("Existence of this file indicates that the Nautilus configuration druid\n"
		  "has been presented.\n\n"
		  "You can manually erase this file to present the druid again.\n");
	
	user_directory = nautilus_get_user_directory ();
	druid_flag_file_name = g_build_filename (user_directory, "first-time-flag", NULL);
	g_free (user_directory);
		
	stream = fopen (druid_flag_file_name, "w");
	if (stream != NULL) {
		fwrite (blurb, sizeof (char), strlen (blurb), stream);
		fclose (stream);
	}
	g_free (druid_flag_file_name);
}
