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

#include <libtrilobite/libtrilobite.h>
#include <gnome.h>
#include <glib.h>
#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
	return_value->description_header = NULL;
	return_value->description = NULL;
	return_value->enabled = TRUE;

	return return_value;

} /* end services_data_new */

static EazelNewsData *
eazel_news_data_new ()
{
	EazelNewsData	*return_value;
	return_value = g_new0 (EazelNewsData, 1);

	return_value->name = NULL;
	return_value->icon = NULL;
	return_value->date = NULL;
	return_value->message = NULL;

	return return_value;

} /* end eazel_news_data_new */

static UpdateNewsData *
update_news_data_new ()
{
	UpdateNewsData	*return_value;
	return_value = g_new0 (UpdateNewsData, 1);

	return_value->name = NULL;
	return_value->version = NULL;
	return_value->priority = NULL;
	return_value->description = NULL;
	return_value->icon = NULL;
	return_value->button_label = NULL;
	return_value->uri = NULL;
	return_value->softcat_uri = NULL;

	return return_value;

} /* end update_news_data_new */

static ServicesData *
parse_a_service (xmlNodePtr node)
{
	ServicesData	*return_value;
	char		*tempbuf;

	return_value = services_data_new ();

	return_value->name = g_strdup (trilobite_xml_get_string (node, "NAME"));
	return_value->icon = g_strdup (trilobite_xml_get_string (node, "ICON"));
	return_value->button_label = g_strdup (trilobite_xml_get_string (node, "BUTTON_LABEL"));
	return_value->uri = g_strdup (trilobite_xml_get_string (node, "URI"));
	return_value->description_header = g_strdup (trilobite_xml_get_string (node, "DESCRIPTION_HEADER"));
	return_value->description = g_strdup (trilobite_xml_get_string (node, "DESCRIPTION"));
	tempbuf = g_strdup (trilobite_xml_get_string (node, "ENABLED"));
	if (tempbuf[0] == 'T' || tempbuf[0] == 't') {
		return_value->enabled = TRUE;
	}
	else if (tempbuf[0] == 'F' || tempbuf[0] == 'f') {
		return_value->enabled = FALSE;
	}
	else {
		g_warning (_("Could not find a valid boolean value for grey_out!"));
		return_value->enabled = FALSE;
	}

	return return_value;

} /* end parse_a_service */

static EazelNewsData *
parse_a_eazel_news_item (xmlNodePtr node)
{
	EazelNewsData *return_value;
	return_value = eazel_news_data_new ();

	return_value->name = g_strdup (trilobite_xml_get_string (node, "NAME"));
	return_value->icon = g_strdup (trilobite_xml_get_string (node, "ICON"));
	return_value->date = g_strdup (trilobite_xml_get_string (node, "DATE"));
	return_value->message = g_strdup (trilobite_xml_get_string (node, "MESSAGE"));

	return return_value;

} /* end parse_a_eazel_news_item */

static UpdateNewsData *
parse_a_update_news_item (xmlNodePtr node)
{
	UpdateNewsData *return_value;
	return_value = update_news_data_new ();

	return_value->name = g_strdup (trilobite_xml_get_string (node, "NAME"));
	return_value->version = g_strdup (trilobite_xml_get_string (node, "VERSION"));
	return_value->priority = g_strdup (trilobite_xml_get_string (node, "PRIORITY"));
	return_value->description = g_strdup (trilobite_xml_get_string (node, "DESCRIPTION"));
	return_value->icon = g_strdup (trilobite_xml_get_string (node, "ICON"));
	return_value->button_label = g_strdup (trilobite_xml_get_string (node, "BUTTON_LABEL"));
	return_value->uri = g_strdup (trilobite_xml_get_string (node, "URI"));
	return_value->softcat_uri = g_strdup (trilobite_xml_get_string (node, "SOFTCAT_URI"));

	return return_value;

} /* end parse_a_update_news_item */

static GList *
build_services_glist_from_xml (xmlNodePtr node)
{
	GList		*return_value;
	xmlNodePtr	service;

	return_value = NULL;
	service = node->xmlChildrenNode;
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
	news_item = node->xmlChildrenNode;
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
	news_item = node->xmlChildrenNode;
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
parse_summary_xml_file (const char *url)
{

	SummaryData	*return_value;
	char		*body;
	int		length;
	xmlDocPtr	doc;
	xmlNodePtr	base;
	xmlNodePtr	child;

	/* fetch remote config file into memory */
	if (! trilobite_fetch_uri (url, &body, &length)) {
		g_assert (_("Could not fetch summary configuration !"));
		return NULL;
	}

	/* <rant> libxml will have a temper tantrum if there is whitespace before the
	*          * first tag.  so we must babysit it.
	*                   */
	while ((length > 0) && (*body <= ' ')) {
		body++, length--;
	}

	doc = xmlParseMemory (body, length);
	 if (doc == NULL) {
		 g_warning ("Invalid data in summary configuration: %s", body);
		 return NULL;
	}

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

	child = doc->root->xmlChildrenNode;

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

