/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-medusa-support.c - Covers for access to medusa
   from nautilus

   Copyright (C) 2001 Eazel, Inc.

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

   Authors: Ramiro Estrugo <ramiro@eazel.com>
            Rebecca Schulman <rebecka@eazel.com>
*/

#include <config.h>
#include "nautilus-medusa-support.h"
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

#include <eel/eel-glib-extensions.h>
#include <eel/eel-string.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>

#ifdef HAVE_MEDUSA
#include <libmedusa/medusa-system-state.h>
#endif

gboolean
nautilus_medusa_services_are_enabled (void)
{
#ifdef HAVE_MEDUSA
	return medusa_system_services_are_enabled ();
#else
	return FALSE;
#endif
}


NautilusCronStatus
nautilus_medusa_check_cron_is_enabled (void)
{
	DIR *proc_directory;
	struct dirent *file;
	char *stat_file_name;
	FILE *stat_file;
	char stat_file_data[128];
	const char *stat_file_process_name;
	int process_number, bytes_read;
	NautilusCronStatus status;

	/* We figure out whether cron is running by reading the proc
	   directory, and checking for a process named or ending with
	   "crond" */

	proc_directory = opendir ("/proc");
	if (proc_directory == NULL) {
		return NAUTILUS_CRON_STATUS_UNKNOWN;
	}

	status = NAUTILUS_CRON_STATUS_UNKNOWN;

	while ((file = readdir (proc_directory)) != NULL) {
		/* Process files have numbers */
		if (!eel_str_to_int (file->d_name, &process_number)) {
			continue;
		}

		/* Since we've seen at least one process file, we can change our state
		 * from "unknown" to "presumed off until proved otherwise".
		 */
		status = NAUTILUS_CRON_STATUS_OFF;

		stat_file_name = g_strdup_printf ("/proc/%d/stat", process_number);
		stat_file = fopen (stat_file_name, "r");
		g_free (stat_file_name);
		
		if (stat_file == NULL) {
			continue;
		}
		
		bytes_read = fread (stat_file_data, 1, sizeof (stat_file_data) - 1, stat_file);
		fclose (stat_file);
		stat_file_data[bytes_read] = '\0';
		
		stat_file_process_name = strchr (stat_file_data, ' ');
		
		if (eel_str_has_prefix (stat_file_process_name, " (crond)") ||
		    eel_str_has_prefix (stat_file_process_name, " (cron)")) {
			status = NAUTILUS_CRON_STATUS_ON;
			break;
		}
	}

	closedir (proc_directory);
	return status;
}

#ifdef HAVE_MEDUSA
static const char *
nautilus_medusa_get_configuration_file_path (void)
{
	return medusa_get_configuration_file_path ();
}
#endif

char *       
nautilus_medusa_get_explanation_of_enabling (void)
{
#ifdef HAVE_MEDUSA	
	return g_strdup_printf (_("If you would like to enable fast searches, you can "
				  "edit the file %s as root. "
				  "Setting the enabled flag to \"yes\" will turn medusa "
				  "services on.\n"
				  "To start indexing and search services right away, you "
				  "should also run the following commands as root:\n"
				  "\n"
				  "medusa-indexd\n"
				  "medusa-searchd\n"
				  "\n"
				  "Fast searches will not be available until an initial "
				  "index of your files has been created.  This may take "
				  "a long time."),
				nautilus_medusa_get_configuration_file_path ());
#else
	return g_strdup_printf (_("Medusa, the application that performs searches, cannot be found on "
				  "your system.  If you have compiled nautilus yourself, "
				  "you will need to install a copy of medusa and recompile nautilus.  "
				  "(A copy of Medusa may be available at ftp://ftp.gnome.org)\n"
				  "If you are using a packaged version of Nautilus, fast searching is "
				  "not available.\n"));
#endif
}
