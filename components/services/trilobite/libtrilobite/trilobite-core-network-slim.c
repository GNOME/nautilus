/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 *  trilobite-core-network: functions for retrieving files from the
 *  network and parsing XML documents
 *  (this version is for the bootstrap installer, it's "slimmed down")
 *
 *  Copyright (C) 2000 Eazel, Inc
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: J Shane Culpepper <pepper@eazel.com>
 *	     Robey Pointer <robey@eazel.com>
 *	     Eskil Heyn Olsen <eskil@eazel.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ghttp.h>
#include <gnome.h>
#include "trilobite-core-utils.h"
#include "trilobite-core-network.h"


/* function for lazy bastards who can't be bothered to figure out the format of the xml they're parsing:
 * it checks for a property with the name, and then if there isn't one, then it tries to find a child
 * with that name instead.
 */
char *
trilobite_xml_get_string (xmlNode *node, const char *name)
{
	char *ret;
	char *tmp;
	xmlNode *child;

	ret = xmlGetProp (node, name);
	if (ret) {
		goto good;
	}
	child = node->xmlChildrenNode;
	while (child) {
		if (g_strcasecmp (child->name, name) == 0) {
			ret = xmlNodeGetContent (child);
			if (ret) {
				goto good;
			}
		}
		child = child->next;
	}
	return NULL;

good:
	tmp = g_strdup (ret);
	xmlFree (ret);
	return tmp;
}

gboolean
trilobite_fetch_uri (const char *uri_text, char **body, int *length)
{
	char *uri = NULL;
        ghttp_request* request;
        ghttp_status status;
	gboolean result = TRUE;

	g_assert (body!=NULL);
	g_assert (uri_text != NULL);
	g_assert (length != NULL);

	uri = g_strdup (uri_text);
        request = NULL;
        (*length) = -1;
        (*body) = NULL;

        if ((request = ghttp_request_new())==NULL) {
                g_warning (_("Could not create an http request !"));
                result = FALSE;
        } 

	/* bootstrap installer does it this way */
	if (result && (g_getenv ("http_proxy") != NULL)) {
		if (ghttp_set_proxy (request, g_getenv ("http_proxy")) != 0) {
			g_warning (_("Proxy: Invalid uri !"));
			result = FALSE;
		}
	}

        if (result && (ghttp_set_uri (request, uri) != 0)) {
                g_warning (_("Invalid uri !"));
                result = FALSE;
        }

	if (result) {
		ghttp_set_header (request, http_hdr_Connection, "close");
		ghttp_set_header (request, http_hdr_User_Agent, trilobite_get_useragent_string (NULL));
	}

        if (result && (ghttp_prepare (request) != 0)) {
                g_warning (_("Could not prepare http request !"));
                result = FALSE;
        }

        if (result && ghttp_set_sync (request, ghttp_async)) {
                g_warning (_("Couldn't get async mode "));
                result = FALSE;
        }

        while (result && (status = ghttp_process (request)) == ghttp_not_done) {
		/*                ghttp_current_status curStat = ghttp_get_status (request); */
		g_main_iteration (FALSE);
        }

        if (result && (ghttp_status_code (request) != 200)) {
                g_warning (_("HTTP error %d \"%s\" on uri %s"), 
			   ghttp_status_code (request),
			   ghttp_reason_phrase (request),
			   uri);
                result = FALSE;
        }
	if (result && (ghttp_status_code (request) != 404)) {
		(*length) = ghttp_get_body_len (request);
		(*body) = g_new0 (char, *length + 1);
		memcpy (*body, ghttp_get_body (request), *length);
		(*body)[*length] = 0;
	} else {
		result = FALSE;
	}

        if (request) {
                ghttp_request_destroy (request);
        }
	
	g_free (uri);

	return result;
}

gboolean
trilobite_fetch_uri_to_file (const char *uri_text, const char *filename)
{
	char *body = NULL;
	int length;
	gboolean result = FALSE;

	result =  trilobite_fetch_uri (uri_text, &body, &length);
	if (result) {
		FILE* file;
		file = fopen (filename, "wb");
		if (file == NULL) {
			g_warning (_("Could not open target file %s"),filename);
			result = FALSE;
		} else {
			fwrite (body, length, 1, file);
		}
		fclose (file);
	} 

	return result;	
}
