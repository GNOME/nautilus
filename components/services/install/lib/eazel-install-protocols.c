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
			const char* target_file) {

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
        length = ghttp_get_body_len (request);
        body = ghttp_get_body (request);
        if (body != NULL) {
                fwrite (body, length, 1, file);
        }
        else {
                g_warning (_("Could not get request body!"));
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

gboolean
eazel_install_fetch_package (EazelInstall *service, 
			     PackageData* package) 
{
	gboolean result;
	char* url;
	char* targetname;
	
	result = FALSE;
	url = NULL;
	
	targetname = g_strdup_printf ("%s/%s",
				      eazel_install_get_tmp_dir (service),
				      rpmfilename_from_packagedata (package));

	switch (eazel_install_get_protocol (service)) {
	case PROTOCOL_FTP:
	case PROTOCOL_HTTP: 
	{
		url = g_strdup_printf ("%s://%s%s/%s",
				       protocol_as_string (eazel_install_get_protocol (service)),
                                       eazel_install_get_hostname (service),
                                       eazel_install_get_rpm_storage_path (service),
                                       rpmfilename_from_packagedata (package));



	}
	break;
	case PROTOCOL_LOCAL:
		url = g_strdup_printf ("%s", rpmfilename_from_packagedata (package));
		break;
	};

	result = eazel_install_fetch_file (service, url, targetname);

	g_free (url);
	g_free (targetname);

	return result;
}

