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
 * Author: J Shane Culpepper <pepper@eazel.com>
 */

#include <config.h>

#include "eazel-summary-shared.h"

#include <gnome.h>
#include <glib.h>
#include <ghttp.h>
#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define USER_AGENT_STRING	"Trilobite"

/* gboolean remove_summary_xml_file (const char *summary_xml_file); */
static GList * build_services_glist_from_xml (xmlNodePtr node);
static GList * build_eazel_news_glist_from_xml (xmlNodePtr node);
static GList * build_update_news_glist_from_xml (xmlNodePtr node);


gboolean
http_fetch_remote_file (char	*url, const char	*target_file)
{

	int		length;
	int		get_failed;
	ghttp_request	*request;
	ghttp_status	status;
	char		*body;
	FILE		*file;

	file = fopen (target_file, "wb");
	get_failed = 0;
	length = -1;
	request = NULL;
	body = NULL;

	request = ghttp_request_new ();
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
		g_warning (_("Could not get async mode !\n"));
		get_failed = 1;
	}

	while ((status = ghttp_process (request)) == ghttp_not_done) {
		ghttp_current_status curStat = ghttp_get_status (request);
		fprintf (stdout, "Progress = %% %f\r", ((float) curStat.bytes_total ?
			 ((float) ((((float) curStat.bytes_read) / (float) curStat.bytes_total)
			 * 100 )) : 100.0));
		fflush (stdout);
		if ((float) curStat.bytes_read == (float) curStat.bytes_total) {
			fprintf (stdout, "\n");
		}
	}

	if (ghttp_status_code (request) != 200) {
		g_warning ("HTTP error: %d %s\n", ghttp_status_code (request), ghttp_reason_phrase (request));
		get_failed = 1;
	}

	if (ghttp_status_code (request) != 404) {
		length = ghttp_get_body_len (request);
		body = ghttp_get_body (request);
		if (body != NULL) {
			fwrite (body, length, 1, file);
		}
		else {
			g_warning (_("Could not get request body !\n"));
			get_failed = 1;
		}
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

static GList *
build_services_glist_from_xml (xmlNodePtr node)
{
	return NULL;
}

static GList *
build_eazel_news_glist_from_xml (xmlNodePtr node)
{
	return NULL;
}

static GList *
build_update_news_glist_from_xml (xmlNodePtr node)
{
	return NULL;
}


SummaryData *
parse_summary_xml_file (const char *summary_xml_file)
{

	SummaryData	*return_value;
	xmlDocPtr	doc;
	xmlNodePtr	base;
	xmlNodePtr	sections;

	g_return_val_if_fail (summary_xml_file != NULL, NULL);

	doc = xmlParseFile (summary_xml_file);

	return_value = g_new0 (SummaryData, 1);

	base = doc->root;

	if (base == NULL) {
		xmlFreeDoc (doc);
		g_warning (_("The summary configuration contains no data!\n"));
		return NULL;
	}

	if (g_strcasecmp (base->name, "SUMMARY_DATA")) {
		g_print (_("Cannot find the SUMMARY_DATA xmlnode!\n"));
		xmlFreeDoc (doc);
		g_warning (_("Bailing from the SUMMARY_DATA parse!\n"));
		return NULL;
	}

	sections = doc->root->childs;

	if (sections == NULL) {
		g_print (_("Could not find any summary configuration data!\n"));
		xmlFreeDoc (doc);
		g_warning (_("Bailing from summary configuration parse!\n"));
		return NULL;
	}
	while (sections) {

		return_value->services_list = build_services_glist_from_xml (sections);
		return_value->eazel_news_list = build_eazel_news_glist_from_xml (sections);
		return_value->update_news_list = build_update_news_glist_from_xml (sections);
	}

	return return_value;

} /* parse_summary_xml_file */
