/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000, 2001  Eazel, Inc
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
 *          Robey Poiner  <robey@eazel.com>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef BUILD_DATE
#define BUILD_DATE "unknown"
#endif

#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <signal.h>
#include <gnome.h>

#include "installer.h"
#include "support.h"
#include "callbacks.h"
#include <libtrilobite/trilobite-core-network.h>

extern int installer_debug;
extern int installer_spam;
extern int installer_output;
extern int installer_test;
extern int installer_force;
extern int installer_dont_ask_questions;
extern char *installer_server;
extern int installer_server_port;
extern char* installer_local;
extern char* installer_package;
extern char* installer_cgi_path;
extern char* installer_tmpdir;
extern char* installer_homedir;
extern char* installer_cache_dir;

static int installer_show_build = 0;
static char *installer_user = NULL;
static int installer_ignore_disk_space = 0;

#define TMPFS_SPACE    	75	/* MB */
#define USRFS_SPACE	50	/* MB */


static const struct poptOption options[] = {
	{"debug", 'd', POPT_ARG_NONE, &installer_debug, 0 , N_("Show debug output"), NULL},
	{"spam", 'x', POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN, &installer_spam, 0, "", NULL},
	{"test", 't', POPT_ARG_NONE, &installer_test, 0, N_("Test the installer without actually installing packages"), NULL},
	{"force", 'f', POPT_ARG_NONE, &installer_force, 0, N_("Forced install"), NULL},
	{"local", '\0', POPT_ARG_STRING, &installer_local, 0, N_("Use local RPMs instead of HTTP server"), "XML-file"},
	{"server", '\0', POPT_ARG_STRING, &installer_server, 0, N_("Specify Eazel installation server"), NULL},
	{"tmpdir", 'T', POPT_ARG_STRING, &installer_tmpdir, 0, N_("Temporary directory to use for downloaded files (default: /tmp)"), "directory"},
	{"homedir", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, &installer_homedir, 0, "", NULL},
	{"user", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, &installer_user, 0, "", NULL},
	{"package", '\0', POPT_ARG_STRING, &installer_package, 0 , N_("Install package"), NULL},
	{"port", '\0', POPT_ARG_INT, &installer_server_port, 0 , N_("Set port number for Eazel installation server (default: 80)"), NULL},
	{"cgi-path", '\0', POPT_ARG_STRING, &installer_cgi_path, 0, N_("Specify CGI path for Eazel installation server"), NULL},
	{"build", 'B', POPT_ARG_NONE, &installer_show_build, 0, N_("Display installer version"), NULL},
	{"batch", '\0', POPT_ARG_NONE, &installer_dont_ask_questions, 0, N_("Solve installation issues without interaction"), NULL},
	{"ignore-disk-space", '\0', POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN, &installer_ignore_disk_space, 0, "", NULL},
	{"cache-dir", 'C', POPT_ARG_STRING, &installer_cache_dir, 0, N_("Look here for cached package files"), "directory"},
	{NULL, '\0', 0, NULL, 0}
};

static void
check_disk_space (void)
{
	struct statfs tmp_fs, usr_fs;
	unsigned long tmp_space, usr_space;

	if ((statfs (installer_tmpdir, &tmp_fs) != 0) || (statfs ("/usr", &usr_fs) != 0)) {
		g_warning ("Can't get free space on /tmp or /usr !");
		return;
	}

	tmp_space = tmp_fs.f_bfree / ((1024*1024) / tmp_fs.f_bsize);
	usr_space = usr_fs.f_bfree / ((1024*1024) / usr_fs.f_bsize);

	if (tmp_space < TMPFS_SPACE) {
		printf ("\nThere isn't enough free space in %s to try this install.\n", installer_tmpdir);
		printf ("It is recommended that you have at least %d MB free.\n", TMPFS_SPACE);
		printf ("You can use the '-T <directory>' command-line option to specify an alternate\n");
		printf ("temporary directory, if you wish.\n");
		printf ("\n");
		printf ("If you scoff at disk space limitations, you can use '--ignore-disk-space'.\n");
		printf ("\n");
		exit (1);
	}
	if (usr_space < USRFS_SPACE) {
		printf ("\nThere isn't enough free space in your /usr directory to try this install.\n");
		printf ("It is recommended that you have at least %d MB free.\n", USRFS_SPACE);
		printf ("\n");
		printf ("If you scoff at disk space limitations, you can use '--ignore-disk-space'.\n");
		printf ("\n");
		exit (1);
	}
}

static void
segv_handler (int unused)
{
	fprintf (stderr, "\n\nSEGV -- CRASH BOOM, THE INSTALLER IS HOSED.\n\n");
	while (1) {
		sleep (1);
	}
}

int
main (int argc, char *argv[])
{
	EazelInstaller *installer;
	char *fake_argv0 = "eazel-installer.sh";
	struct stat statbuf;

#ifdef ENABLE_NLS
	bindtextdomain ("eazel-installer", GNOMELOCALEDIR);
	textdomain ("eazel-installer");
#endif
	argv[0] = fake_argv0;

	if (argc < 2) {
		printf ("%s: incorrect number of parameters\n", argv[0]);
		exit (0);
	}

	if (strcmp (argv[1], "--build") == 0) {
		/* skip the crap. */
		printf ("\nEazel Installer v%s (build %s)\n\n", VERSION, BUILD_DATE);
		exit (0);
	}

	gnome_init_with_popt_table ("eazel-installer", VERSION, argc, argv, options, 0, NULL);

	signal (SIGSEGV, segv_handler);
	gdk_rgb_init ();

	if (installer_show_build) {
		printf ("\nEazel Installer v%s (build %s)\n\n", VERSION, BUILD_DATE);
		exit (0);
	}

	if (installer_user == NULL) {
		printf ("\nThe --user flag is mandatory.\n");
		exit (0);
	}

	if (installer_spam) {
		installer_debug = 1;
	}

	if (installer_homedir == NULL) {
		struct passwd *passwd_entry;

		passwd_entry = getpwnam (installer_user);
		if (passwd_entry != NULL) {
			installer_homedir = g_strdup (passwd_entry->pw_dir);
		} else {
			/* give up */
			printf ("*** Unable to find user's homedir: using '/'\n");
			installer_homedir = g_strdup ("/");
		}
	}

	if (stat (installer_tmpdir, &statbuf) != 0) {
		printf ("*** Unable to access the temporary directory %s\n", installer_tmpdir);
		printf ("    You may need to specify a directory using -T\n");
		exit (1);
	}

	if (! installer_ignore_disk_space) {
		check_disk_space ();
	}

	installer = eazel_installer_new ();

	gtk_main ();

	gtk_object_unref (GTK_OBJECT (installer->service));
	gtk_object_unref (GTK_OBJECT (installer->problem));

	printf ("Exiting\n");
	g_mem_profile ();
	return 0;
}


/* Dummy functions to make linking work 
   (this is the type of code your mother warned you about) */

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
