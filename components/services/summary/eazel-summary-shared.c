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
static SummaryData * summary_data_new (void);
static ServicesData * services_data_new (void);
static EazelNewsData * eazel_news_data_new (void);
static UpdateNewsData * update_news_data_new (void);
static ServicesData * parse_a_service (xmlNodePtr node);
static EazelNewsData * parse_a_eazel_news_item (xmlNodePtr node);
static UpdateNewsData * parse_a_update_news_item (xmlNodePtr node);


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

static SummaryData *
summary_data_new ()
{
	SummaryData	*return_value;
	return_value = g_new0 (SummaryData, 1);

	return_value->services_list = NULL;
	return_value->eazel_news_list = NULL;
	return_value->update_news_list = NULL;

	return return_value;

} /* end summary_data_new */

static ServicesData *
services_data_new ()
{
	ServicesData	*return_value;
	return_value = g_new0 (ServicesData, 1);

	return_value->name = NULL;
	return_value->icon = NULL;
	return_value->button_label = NULL;
	return_value->description = NULL;

	return return_value;

} /* end services_data_new */

static EazelNewsData *
eazel_news_data_new ()
{
	EazelNewsData	*return_value;
	return_value = g_new0 (EazelNewsData, 1);

	return_value->name = NULL;
	return_value->icon = NULL;
	return_value->message = NULL;

	return return_value;

} /* end eazel_news_data_new */

static UpdateNewsData *
update_news_data_new ()
{
	UpdateNewsData	*return_value;
	return_value = g_new0 (UpdateNewsData, 1);

	return_value->name = NULL;
	return_value->priority = NULL;
	return_value->description = NULL;
	return_value->icon = NULL;
	return_value->install_uri = NULL;

	return return_value;

} /* end update_news_data_new */

static ServicesData *
parse_a_service (xmlNodePtr node)
{
	ServicesData *return_value;
	return_value = services_data_new ();
	return return_value;

} /* end parse_a_service */

static EazelNewsData *
parse_a_eazel_news_item (xmlNodePtr node)
{
	EazelNewsData *return_value;
	return_value = eazel_news_data_new ();
	return return_value;

} /* end parse_a_service */

static UpdateNewsData *
parse_a_update_news_item (xmlNodePtr node)
{
	UpdateNewsData *return_value;
	return_value = update_news_data_new ();
	return return_value;

} /* end parse_a_service */

static GList *
build_services_glist_from_xml (xmlNodePtr node)
{
	GList		*return_value;
	xmlNodePtr	service;

	return_value = NULL;
	service = node->childs;
	if (service == NULL) {
		g_warning (_("There is no service data !\n"));
		return NULL;
	}

	while (service) {
		ServicesData	*sdata;

		sdata = parse_a_service (service);
		return_value = g_list_append (return_value, sdata);
		service = service->next;
	}

	return return_value;

} /* end build_services_glist_from_xml */

static GList *
build_eazel_news_glist_from_xml (xmlNodePtr node)
{
	GList		*return_value;
	xmlNodePtr	news_item;

	return_value = NULL;
	news_item = node->childs;
	if (news_item == NULL) {
		g_warning (_("There is no eazel news data !\n"));
		return NULL;
	}

	while (news_item) {
		EazelNewsData	*ndata;

		ndata = parse_a_eazel_news_item (news_item);
		return_value = g_list_append (return_value, ndata);
		news_item = news_item->next;
	}

	return return_value;

} /* end build_eazel_news_glist_from_xml */

static GList *
build_update_news_glist_from_xml (xmlNodePtr node)
{
	GList		*return_value;
	xmlNodePtr	news_item;

	return_value = NULL;
	news_item = node->childs;
	if (news_item == NULL) {
		g_warning (_("There is no eazel news data !\n"));
		return NULL;
	}

	while (news_item) {
		UpdateNewsData	*ndata;

		ndata = parse_a_update_news_item (news_item);
		return_value = g_list_append (return_value, ndata);
		news_item = news_item->next;
	}

	return return_value;

} /* end build_update_news_glist_from_xml */


SummaryData *
parse_summary_xml_file (const char *summary_xml_file)
{

	SummaryData	*return_value;
	xmlDocPtr	doc;
	xmlNodePtr	base;
	xmlNodePtr	child;

	g_return_val_if_fail (summary_xml_file != NULL, NULL);

	doc = xmlParseFile (summary_xml_file);

	return_value = summary_data_new ();

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

	child = doc->root->childs;

	if (child == NULL) {
		g_print (_("Could not find any summary configuration data!\n"));
		xmlFreeDoc (doc);
		g_warning (_("Bailing from summary configuration parse!\n"));
		return NULL;
	}

	while (child) {
		if (g_strcasecmp (child->name, "SERVICES") == 0) {
			return_value->services_list = build_services_glist_from_xml (child);
		}
		if (g_strcasecmp (child->name, "EAZEL_NEWS") == 0) {
			return_value->eazel_news_list = build_eazel_news_glist_from_xml (child);
		}
		if (g_strcasecmp (child->name, "UPDATE_NEWS") == 0) {
			return_value->update_news_list = build_update_news_glist_from_xml (child);
		}
		child = child->next;
	}

	return return_value;

} /* parse_summary_xml_file */
