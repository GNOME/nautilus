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
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 *
 */

/*
  static link with
gcc -static -g -Wall -Wno-uninitialized -Wchar-subscripts -Wmissing-declarations -Wmissing-prototypes -Wnested-externs \
-Wpointer-arith -Wno-sign-compare -Wsign-promo -o .libs/eazel-alt-install eazel-alt-install.o \
../../../../components/services/trilobite/libtrilobite/.libs/libtrilobite.so -L/gnome/lib -L/usr/lib \
../../../../components/services/install/lib/libinstall.a -Wl,--rpath -Wl,/gnome/lib \
-lgnome -lglib -ldl -lghttp -lrpm -lz -lbz2 -lpopt
 */

#include <config.h>
#include <gnome.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>

#include "eazel-install-public.h"
#include <libtrilobite/helixcode-utils.h>

#define PACKAGE_FILE_NAME "package-list.xml"
#define DEFAULT_CONFIG_FILE "/var/eazel/services/eazel-services-config.xml"

#define DEFAULT_HOSTNAME "vorlon.eazel.com"
#define DEFAULT_PORT_NUMBER 80
#define DEFAULT_PROTOCOL PROTOCOL_HTTP
#define DEFAULT_TMP_DIR "/tmp/eazel-install"
#define DEFAULT_RPMRC "/usr/lib/rpm/rpmrc"
#define DEFAULT_REMOTE_PACKAGE_LIST "/package-list.xml"
#define DEFAULT_REMOTE_RPM_DIR "/RPMS"
#define DEFAULT_LOG_FILE "/tmp/eazel-install/log"

int     arg_dry_run,
	arg_http,
	arg_ftp,
	arg_local;
char    *arg_server,
	*arg_config_file;

static const struct poptOption options[] = {
	{"dry", 'd', POPT_ARG_NONE, &arg_dry_run, 0, N_("Test run"), NULL},
	{"server", '\0', POPT_ARG_STRING, &arg_server, 0, N_("Specify server"), NULL},
	{"http", 'h', POPT_ARG_NONE, &arg_http, 0, N_("Use http"), NULL},
	{"ftp", 'f', POPT_ARG_NONE, &arg_ftp, 0, N_("Use ftp"), NULL},
	{"local", 'l', POPT_ARG_NONE, &arg_local, 0, N_("Use local"), NULL},
	{"config", '\0', POPT_ARG_STRING, &arg_config_file, 0, N_("Specify config file"), NULL},
	{NULL, '\0', 0, NULL, 0}
};

static void
set_parameters_from_command_line (EazelInstall *service)
{
	/* We only want 1 protocol type */
	if (arg_http + arg_ftp + arg_local > 1) {
			fprintf (stderr, "*** You cannot specify more then one protocol type.\n");
			exit (1);
	}

	/* Set the procol */
	if (arg_http) {
		eazel_install_set_protocol (service, PROTOCOL_HTTP);
	} else if (arg_ftp) {
		eazel_install_set_protocol (service, PROTOCOL_FTP);
	} else if (arg_local) {
		eazel_install_set_protocol (service, PROTOCOL_LOCAL);
	}


	fprintf (stdout, "protocol is %d\n", eazel_install_get_protocol (service));
	fprintf (stdout, "tmpdir is %s\n", eazel_install_get_tmp_dir (service));
	fprintf (stdout, "pkg_list is %s\n", eazel_install_get_package_list (service));
}

static void 
eazel_install_progress (EazelInstall *service, 
			const char *name,
			int amount, 
			int total,
			gpointer data) 
{
	fprintf (stdout, "INSTALL Progress - %% %f\r", (total ? ((float)
							 ((((float) amount) / total) * 100))
						: 100.0));
	fflush (stdout);
	if (amount == total) {
		fprintf (stdout, "\n");
	}
}


static void 
eazel_download_progress (EazelInstall *service, 
			 const char *name,
			 int amount, 
			 int total,
			 gpointer data) 
{
	fprintf (stdout, "DOWNLOAD Progress - %% %f\r", (total ? ((float)
							 ((((float) amount) / total) * 100))
						: 100.0));
	fflush (stdout);
	if (amount == total) {
		fprintf (stdout, "\n");
	}

/*
                fprintf (stdout, "Progress - %% %f\r", ((float)
                         curStat.bytes_total ? ((float) ((((float)
                         curStat.bytes_read) / (float) curStat.bytes_total)
                         * 100 )) : 100.0));
                fflush (stdout);
                if ((float) curStat.bytes_read == (float) curStat.bytes_total) {                        fprintf (stdout, "\n");
                }
*/
}


int main(int argc, char *argv[]) {
	EazelInstall *service;

	gnome_init_with_popt_table ("trilobite-eazel-time-service-cli", "1.0",argc, argv, options, 0, NULL);	

	if (check_for_root_user() == FALSE) {
		fprintf (stderr, "*** This tool requires root access.\n");
	}

	if (check_for_redhat () == FALSE) {
		fprintf (stderr, "*** This tool can only be used on RedHat.\n");
	}

	if (arg_config_file == NULL) {
		arg_config_file = g_strdup (DEFAULT_CONFIG_FILE);
	}
	service = eazel_install_new_with_config (arg_config_file);
	g_assert (service != NULL);

	gtk_signal_connect (GTK_OBJECT (service), "download_progress", eazel_download_progress, NULL);
	gtk_signal_connect (GTK_OBJECT (service), "install_progress", eazel_install_progress, NULL);

	eazel_install_open_log (service, DEFAULT_LOG_FILE);
	eazel_install_set_hostname (service, DEFAULT_HOSTNAME);
	eazel_install_set_rpmrc_file (service, DEFAULT_RPMRC);
	eazel_install_set_package_list_storage_path (service, DEFAULT_REMOTE_PACKAGE_LIST);
	eazel_install_set_rpm_storage_path (service, DEFAULT_REMOTE_RPM_DIR);
	eazel_install_set_tmp_dir (service, DEFAULT_TMP_DIR);
	eazel_install_set_port_number (service, DEFAULT_PORT_NUMBER);
	eazel_install_set_protocol (service, DEFAULT_PROTOCOL);
	
	set_parameters_from_command_line (service);

	eazel_install_new_packages (service);
							
	eazel_install_destroy (GTK_OBJECT (service));

	return 0;
};
