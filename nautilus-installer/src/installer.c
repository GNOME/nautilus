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
#include <libtrilobite/helixcode-utils.h>
#include "interface.h"
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <sys/utsname.h>

#define EAZEL_SERVICES_DIR_HOME "/var/eazel"
#define EAZEL_SERVICES_DIR EAZEL_SERVICES_DIR_HOME "/services"

static char *failure_info = NULL;

static void
make_dirs ()
{
	int retval;
	if (! g_file_test (EAZEL_SERVICES_DIR, G_FILE_TEST_ISDIR)) {
		if (! g_file_test (EAZEL_SERVICES_DIR_HOME, G_FILE_TEST_ISDIR)) {
			retval = mkdir (EAZEL_SERVICES_DIR_HOME, 0755);		       
			if (retval < 0) {
				if (errno != EEXIST) {
					g_error (_("*** Could not create services directory (%s)! ***\n"), 
						 EAZEL_SERVICES_DIR_HOME);
				}
			}
		}

		retval = mkdir (EAZEL_SERVICES_DIR, 0755);
		if (retval < 0) {
			if (errno != EEXIST) {
				g_error (_("*** Could not create services directory (%s)! ***\n"), 
					 EAZEL_SERVICES_DIR);
			}
		}
	}
}

void
check_system (EazelInstaller *installer)
{
	DistributionInfo dist;

	dist = trilobite_get_distribution ();

#ifndef NAUTILUS_INSTALLER_RELEASE
	if (!installer->test) {
		GnomeDialog *d;
		d = GNOME_DIALOG (gnome_warning_dialog_parented (_("This is a warning, you're running\n"
								   "the installer for real, without \n"
								   "the --test flag... Beware!"),
								 GTK_WINDOW (installer->window)));
		gnome_dialog_run_and_close (d);
	} 
#endif

	if (dist.name != DISTRO_REDHAT) {
		GnomeDialog *d;
		d = GNOME_DIALOG (gnome_warning_dialog_parented (_("This preview installer only works\n"
								   "for RPM based systems. You will have\n"
								   "to download the source yourself."),
								 GTK_WINDOW (installer->window)));
		gnome_dialog_run_and_close (d);
		exit (1);
	}

	if (!g_file_test ("/etc/pam.d/helix-update", G_FILE_TEST_ISFILE)) {
		GnomeDialog *d;
		d = GNOME_DIALOG (gnome_warning_dialog_parented (_("You do not have HelixCode gnome installed.\n"
								   "This means I will install the required parts\n"
								   "for you, but you might want to abort the\n"
								   "installer and go to http://www.helixcode.com\n"
								   "and download the full HelixCode Gnome\n"
								   "installation"),
								 GTK_WINDOW (installer->window)));
		gnome_dialog_run_and_close (d);
	}
}

#if 0
void
revert_nautilus_install (EazelInstall *service)
{
	DIR *dirent;
	struct dirent *de;

	dirent = opendir (EAZEL_SERVICES_DIR);
	
	while (de = readdir (dirent)) {
		if (strncmp (de->d_name, "transaction.", 12)==0) {
			eazel_install_revert_transaction_from_file (service, de->d_name);
			unlink (de->d_name);
		}
	}
}
#endif 

void 
eazel_installer_do_install (EazelInstaller *installer,
			    GList *categories)
{
       	
	eazel_install_install_packages (installer->service, categories, NULL);
#if 0
	revert_nautilus_install (service, NULL);
#endif

	if (installer->failure_info && strlen (installer->failure_info)>1) {
		if (installer->debug) {
			fprintf (stdout, "ERROR :\n%s", installer->failure_info);
		}
		gnome_error_dialog_parented (installer->failure_info, GTK_WINDOW (installer->window));
	} else {
	}
}


/* Dummy functions to make linking work */

const gpointer oaf_popt_options = NULL;
gpointer oaf_init (int argc, char *argv[]) {}
int bonobo_init (gpointer a, gpointer b, gpointer c) {};
