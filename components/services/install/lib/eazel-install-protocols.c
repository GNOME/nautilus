/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 * Copyright (C) 2000 Helix Code, Inc
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
http_fetch_xml_package_list (const char* hostname,
                             int port,
                             const char* path,
                             const char* pkg_list_file) {

	GInetAddr* addr;
	GTcpSocket* socket;
	GIOChannel* iochannel;
	char* request;
	char* xmlbuf;
	GIOError error;
	guint bytes;
	xmlDocPtr doc;

	xmlbuf = "";

	/* Create the socket address */

	addr = gnet_inetaddr_new (hostname, port);
	g_assert (addr != NULL);

	/* Create the socket */
	socket = gnet_tcp_socket_new (addr);
	g_assert (socket != NULL);

	/* Get an IOChannel */
	iochannel = gnet_tcp_socket_get_iochannel (socket);
	g_assert (iochannel != NULL);

	/* Make the request */
	request = g_strdup_printf ("GET %s HTTP/1.0\r\n\r\n", path);
	error = gnet_io_channel_writen (iochannel, request, strlen(request), &bytes);
	g_free(request);

	if (error != G_IO_ERROR_NONE) {
		g_warning("Unable to connect to host: %d\n", error);
	}

	/* Read the returned info */
	while (1) {
		char* buffer;

		buffer = g_new0(char, 1024);
		
		error = g_io_channel_read(iochannel, buffer, sizeof(buffer), &bytes);
		if (error != G_IO_ERROR_NONE) {
			g_warning ("Read Error: %d\n", error);
			break;
		}
		if (bytes == 0) {
			break;
		}
		xmlbuf = g_strconcat (xmlbuf, buffer, NULL);
		g_free (buffer);
	}
	g_io_channel_unref (iochannel);
	gnet_tcp_socket_delete (socket);

	if (strstr (xmlbuf, "HTTP1.0 404") || strstr (xmlbuf, "HTTP/1.1 404")) {
		fprintf (stderr, "***File %s not found !***\n", path);
		return FALSE;
	}
	if (strstr (xmlbuf, "HTTP1.0 403") || strstr (xmlbuf, "HTTP/1.1 403")) {
		fprintf (stderr, "***Server denied access !***\n");
		return FALSE;
	}
	if (strstr (xmlbuf, "HTTP1.0 400") || strstr (xmlbuf, "HTTP/1.1 400")) {
		fprintf (stderr, "***Server could not understand the request !***\n");
		return FALSE;
	}

	doc = prune_xml (xmlbuf);
	if (!doc) {
		fprintf (stderr, "***Unable to read package file !***\n");
		return FALSE;
	}

	xmlSaveFile (pkg_list_file, doc);
	xmlFreeDoc (doc);
	return TRUE;

} /* end http_fetch_xml_package_list */

gboolean
http_fetch_remote_file (char* url, const char* target_file) {

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
                fprintf (stdout, "Progress - %% %f\r", ((float)
                         curStat.bytes_total ? ((float) ((((float)
                         curStat.bytes_read) / (float) curStat.bytes_total)
                         * 100 )) : 100.0));
                fflush (stdout);
                if ((float) curStat.bytes_read == (float) curStat.bytes_total) {                        fprintf (stdout, "\n");
                }
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

