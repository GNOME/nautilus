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

gboolean
http_fetch_remote_file (EazelInstall *service,
			char* url, 
			const char* target_file) {

        int length, get_failed;
        ghttp_request* request;
        ghttp_status status;
        char* body;
        FILE* file;

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
                g_warning (_("Could not create an http request !\n"));
                get_failed = 1;
        }

        if (ghttp_set_uri (request, url) != 0) {
                g_warning (_("Invalid uri !\n"));
                get_failed = 1;
        }

        ghttp_set_header (request, http_hdr_Connection, "close");
        ghttp_set_header (request, http_hdr_User_Agent, USER_AGENT_STRING);
        if (ghttp_prepare (request) != 0) {
                g_warning (_("Could not prepare http request !\n"));
                get_failed = 1;
        }
        if (ghttp_set_sync (request, ghttp_async)) {
                g_warning (_("Couldn't get async mode \n"));
                get_failed = 1;
        }

        while ((status = ghttp_process (request)) == ghttp_not_done) {
                ghttp_current_status curStat = ghttp_get_status (request);
		eazel_install_emit_download_progress (service, target_file, curStat.bytes_read, curStat.bytes_total);		
        }
        if (ghttp_status_code (request) != 200) {
                g_warning ("HTTP error: %d %s\n", ghttp_status_code (request),
                         ghttp_reason_phrase (request));
                get_failed = 1;
        }
        length = ghttp_get_body_len (request);
        body = ghttp_get_body (request);
        if (body != NULL) {
                fwrite (body, length, 1, file);
        }
        else {
                g_warning (_("Could not get request body!\n"));
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

