/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 * Copyright (C) 1998-1999 James Henstridge
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
 * Authors: J Shane Culpepper <pepper@eazel.com>
 */

/* eazel-install - services command line install/update/uninstall
 * component.  This program will parse the eazel-services-config.xml
 * file and install a services generated package-list.xml.
 */

#include "eazel-install-protocols.h"
#include <config.h>


gboolean http_fetch_remote_file (EazelInstall *service,
				 char* url, 
				 const char* target_file);
gboolean ftp_fetch_remote_file (EazelInstall *service,
				char* url, 
				const char* target_file);
gboolean local_fetch_remote_file (EazelInstall *service,
				  char* url, 
				  const char* target_file);

gboolean
http_fetch_remote_file (EazelInstall *service,
			char* url, 
			const char* target_file) 
{
        int length, get_failed;
        ghttp_request* request;
        ghttp_status status;
        char* body;
        FILE* file;
	int total_bytes;

	g_message ("Downloading %s...", url);

        file = fopen (target_file, "wb");
        get_failed = 0;
        length = -1;
        request = NULL;
        body = NULL;

	if (file == NULL) {
		get_failed = 1;
		g_warning ("Could not open target file %s",target_file);
		return FALSE;

	}

        request = ghttp_request_new();
        if (!request) {
                g_warning (_("Could not create an http request !"));
                get_failed = 1;
        }

        if (ghttp_set_uri (request, url) != 0) {
                g_warning (_("Invalid uri !"));
                get_failed = 1;
        }

        ghttp_set_header (request, http_hdr_Connection, "close");
        ghttp_set_header (request, http_hdr_User_Agent, USER_AGENT_STRING);
        if (ghttp_prepare (request) != 0) {
                g_warning (_("Could not prepare http request !"));
                get_failed = 1;
        }
        if (ghttp_set_sync (request, ghttp_async)) {
                g_warning (_("Couldn't get async mode "));
                get_failed = 1;
        }

        while ((status = ghttp_process (request)) == ghttp_not_done) {
                ghttp_current_status curStat = ghttp_get_status (request);
		total_bytes = curStat.bytes_total;
		eazel_install_emit_download_progress (service, target_file, curStat.bytes_read, curStat.bytes_total);
        }
	eazel_install_emit_download_progress (service, target_file, total_bytes, total_bytes);		
        if (ghttp_status_code (request) != 200) {
                g_warning ("HTTP error: %d %s", ghttp_status_code (request),
                         ghttp_reason_phrase (request));
                get_failed = 1;
        }
	if (ghttp_status_code (request) != 404) {
		length = ghttp_get_body_len (request);
		body = ghttp_get_body (request);
		if (body != NULL) {
			fwrite (body, length, 1, file);
		} else {
			g_warning (_("Could not get request body!"));
			get_failed = 1;
		}
	} else {
		get_failed = 1;
	}

        if (request) {
                ghttp_request_destroy (request);
        }
        fclose (file);

        if (get_failed != 0) {
		return FALSE;
        }
	else {
		return TRUE;
	}
} /* end http_fetch_remote_file */

gboolean
ftp_fetch_remote_file (EazelInstall *service,
			char* url, 
			const char* target_file) 
{
	g_message ("Downloading %s...", url);
	g_warning (_("FTP not supported yet"));
	return FALSE;
}


gboolean
local_fetch_remote_file (EazelInstall *service,
			 char* url, 
			 const char* target_file) 
{
	gboolean result;
	
	g_message ("Checking local file %s...", target_file);
	result = FALSE;
	if (access (target_file, R_OK|W_OK) == 0) {
		eazel_install_emit_download_progress (service, target_file, 100, 100);
		result = TRUE;
	} 
	return result;
}

gboolean
eazel_install_fetch_file (EazelInstall *service,
			  char* url, 
			  const char* target_file) 
{
	gboolean result;
	
	result = FALSE;

	g_return_val_if_fail (url!=NULL, FALSE);
	g_return_val_if_fail (target_file!=NULL, FALSE);
	
	switch (eazel_install_get_protocol (service)) {
	case PROTOCOL_HTTP:
		result = http_fetch_remote_file (service, url, target_file);
		break;
	case PROTOCOL_FTP:
		result = ftp_fetch_remote_file (service, url, target_file);
		break;
	case PROTOCOL_LOCAL:
		result = local_fetch_remote_file (service, url, target_file);
		break;
	}
	return result;
}


static const char*
filename_from_url (char *url)
{
	static char *filename = NULL;
	char *ptr;

	//g_return_val_if_fail (url!=NULL, NULL);

	g_free (filename);

	ptr = url + strlen (url);
	while ((ptr != url) && (*ptr != '/')) { 
		ptr--; 
	}
	filename = g_strdup (ptr + 1);
	
	return filename;
}

gboolean
eazel_install_fetch_package (EazelInstall *service, 
			     PackageData* package) 
{
	gboolean result;
	char* url;
	char* targetname;
	
	result = FALSE;
	url = NULL;
	

	switch (eazel_install_get_protocol (service)) {
	case PROTOCOL_FTP:
	case PROTOCOL_HTTP: 
	{
		url = get_url_for_package (service, package);
	}
	break;
	case PROTOCOL_LOCAL:
		url = g_strdup_printf ("%s", rpmfilename_from_packagedata (package));
		break;
	};

	if (url == NULL) {
		g_warning (_("Could not get a URL for %s"), rpmfilename_from_packagedata (package));
	} else {
		/* FIXME: bugzilla.eazel.com 1315
		   Loose the check once a proper rpmsearch.cgi is up and running */
		if (filename_from_url (url) && strlen (filename_from_url (url))>1) {
			targetname = g_strdup_printf ("%s/%s",
						      eazel_install_get_tmp_dir (service),
						      filename_from_url (url));
			package->filename = g_strdup (filename_from_url (url));
			result = eazel_install_fetch_file (service, url, targetname);
			g_free (targetname);
		}
		g_free (url);
	}
	

	return result;
}

static void
add_to_url (char **url,
	    char *cgi_string,
	    char *val)
{
	char *tmp;

	g_assert (url != NULL);
	g_assert ((*url) != NULL);
	g_assert (cgi_string != NULL);

	if (val) {
		tmp = g_strconcat (*url, cgi_string, val, NULL);
		g_free (*url);
		(*url) = tmp;
	}
}

char*
get_url_for_package  (EazelInstall *service, 
		      PackageData *package)
{
	char *search_url;
	char *url;
        ghttp_status status;
        ghttp_request* request;

	url = NULL;
	search_url = get_search_url_for_package (service, package);
	g_message ("Search URL: %s", search_url);

        request = ghttp_request_new();
        if (request == NULL) {
                g_warning (_("Could not create an http request !"));
        } else {
	
		if (ghttp_set_uri (request, search_url) != 0) {
			g_warning (_("Invalid uri"));
		} else {
			if (ghttp_prepare (request) != 0) {
				g_warning (_("Could not prepare http request !"));
			} else {
		
				status = ghttp_process (request);
				
				switch (status) {
				case ghttp_error:					
				case ghttp_not_done:
					g_warning (_("Could not retrieve a URL for %s"), rpmfilename_from_packagedata (package));
				case ghttp_done:
					if (ghttp_status_code (request) != 404) {
						url = g_strdup (ghttp_get_body (request));
						if (url) {
							url [ ghttp_get_body_len (request)] = 0;
						}
					} else {
						/*
						url = g_strdup_printf("http://%s%s/%s",
								      eazel_install_get_server (service),
								      eazel_install_get_rpm_storage_path (service),
								      rpmfilename_from_packagedata (package));
						*/
						url = NULL;
					}
					break;
				}
			}
		}
		
                ghttp_request_destroy (request);
        }
	
	g_free (search_url);
	return url;
}


char* get_search_url_for_package (EazelInstall *service, 
				  PackageData *pack)
{
	char *url;
	url = g_strdup_printf ("http://%s:%d/cgi-bin/rpmsearch.cgi",
			       eazel_install_get_server (service),
			       eazel_install_get_server_port (service));
	add_to_url (&url, "?name=", pack->name);
	add_to_url (&url, "&arch=", pack->archtype);
	/* add_to_url (&url, "&version>=", pack->version); */

	switch (eazel_install_get_protocol (service)) {
	case PROTOCOL_HTTP:
		add_to_url (&url, "&protocol=", "http");
		break;
	case PROTOCOL_FTP:
		add_to_url (&url, "&protocol=", "ftp");
		break;
	default:
		break;
	}
/*
  FIXME: bugzilla.eazel.com 1333
  We need to sent distro name at some point. Depends on the rpmsearch cgi script
	if (pack->name != DISTRO_UNKNOWN) {
		char *distro;
		distro = g_strconcat ("\"", trilobite_get_distribution_name (pack->distribution, TRUE), "\"", NULL);
		add_to_url (&url, "&distro=", distro);
		g_free (distro);
	}
*/
	return url;
}
