/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
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
 * Authors: Eskil Heyn Olsen  <eskil@eazel.com>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "installer.h"
#include "support.h"
#include "callbacks.h"
#include <libtrilobite/helixcode-utils.h>

extern int installer_debug;
extern int installer_output;
extern int installer_test;
extern int installer_force;
extern int installer_no_helix;
extern char *installer_server;
extern int installer_server_port;
extern char* installer_local;
extern char* installer_cgi_path;

static const struct poptOption options[] = {
	{"debug", 'd', POPT_ARG_NONE, &installer_debug, 0 , N_("Show debug output"), NULL},
	{"test", 't', POPT_ARG_NONE, &installer_test, 0, N_("Test the installer without actually installing packages"), NULL},
	{"force", 'f', POPT_ARG_NONE, &installer_force, 0, N_("Forced install"), NULL},
	{"local", '\0', POPT_ARG_STRING, &installer_local, 0, N_("Use local, specify XML file to use"), NULL},
	{"server", '\0', POPT_ARG_STRING, &installer_server, 0, N_("Specify Eazel installation server"), NULL},
#if 0
	{"nohelix", '\0', POPT_ARG_NONE, &installer_no_helix, 0, N_("Assume no-helix"), NULL},
#endif
	{"port", '\0', POPT_ARG_INT, &installer_server_port, 0 , N_("Set port number for Eazel installation server (default: 80)"), NULL},
	{"cgi-path", '\0', POPT_ARG_STRING, &installer_cgi_path, 0, N_("Specify CGI path for Eazel installation server"), NULL},
	{NULL, '\0', 0, NULL, 0}
};

int
main (int argc, char *argv[])
{
	EazelInstaller *installer;

#ifdef ENABLE_NLS
	bindtextdomain ("nautilus-installer", GNOMELOCALEDIR);
	textdomain ("nautilus-installer");
#endif
	gnome_init_with_popt_table ("eazel-installer", VERSION, argc, argv, options, 0, NULL);

	installer = eazel_installer_new ();

	gtk_main ();
	return 0;
}


/* Dummy functions to make linking work */

const gpointer oaf_popt_options = NULL;
gpointer oaf_init (int argc, char *argv[]) { return NULL; }
int bonobo_init (gpointer a, gpointer b, gpointer c) { return 0; };
char *nautilus_pixmap_file (const char *a) { return NULL; };

/* stub out esound */
int esd_open_sound (const char *host) { return -1; }
int esd_sample_getid (int esd, const char *name) { return -1; }
int esd_sample_play (int esd, int sample) { return -1; }
int esd_sample_cache (int esd, void *format, const int rate, const int length, const char *name) { return -1; }
int esd_sample_free (int esd, int sample) { return -1; }
int esd_confirm_sample_cache (int esd) { return -1; }
int esd_close (int esd) { return -1; }

void *afOpenFile (const char *filename, const char *mode, int setup) { return NULL; }
int afGetFrameCount (void *file, int track) { return 0; }
int afGetChannels (void *file, int track) { return 0; }
double afGetRate (void *file, int track) { return 0.0; }
void afGetSampleFormat (void *file, int track, int *sampfmt, int *sampwidth) { }
int afReadFrames (void *file, int track, void *buffer, int frameCount) { return 0; }
int afCloseFile (void *file) { return 0; }
