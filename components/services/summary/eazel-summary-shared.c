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
#include <libtrilobite/trilobite-file-utilities.h>
#include <gnome.h>
#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

static GList * build_services_glist_from_xml (xmlNodePtr node);
static GList * build_eazel_news_glist_from_xml (xmlNodePtr node);
static GList * build_update_news_glist_from_xml (xmlNodePtr node);
static SummaryData * summary_data_new (void);
static ServicesData * services_data_new (void);
static EazelNewsData * eazel_news_data_new (void);
static UpdateNewsData * update_news_data_new (void);
static ServicesData * parse_service (xmlNodePtr node);
static EazelNewsData * parse_eazel_news_item (xmlNodePtr node);
static UpdateNewsData * parse_update_news_item (xmlNodePtr node);


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
parse_service (xmlNodePtr node)
{
	ServicesData	*return_value;
	char		*tempbuf;

	return_value = services_data_new ();

	return_value->name = g_strdup (trilobite_xml_get_string (node, "NAME"));

	if (return_value->name == NULL) {
		g_free (return_value);
		return NULL;
	}

	return_value->icon = g_strdup (trilobite_xml_get_string (node, "ICON"));
	return_value->button_label = g_strdup (trilobite_xml_get_string (node, "BUTTON_LABEL"));
	return_value->uri = g_strdup (trilobite_xml_get_string (node, "URI"));
	return_value->description_header = g_strdup (trilobite_xml_get_string (node, "DESCRIPTION_HEADER"));
	return_value->description = g_strdup (trilobite_xml_get_string (node, "DESCRIPTION"));
	tempbuf = g_strdup (trilobite_xml_get_string (node, "ENABLED"));
	if (tempbuf != NULL && (tempbuf[0] == 'T' || tempbuf[0] == 't')) {
		return_value->enabled = TRUE;
	} else if (tempbuf != NULL && (tempbuf[0] == 'F' || tempbuf[0] == 'f')) {
		return_value->enabled = FALSE;
	} else {
		return_value->enabled = FALSE;
	}

	return return_value;

} /* end parse_service */

static EazelNewsData *
parse_eazel_news_item (xmlNodePtr node)
{
	EazelNewsData *return_value;
	return_value = eazel_news_data_new ();

	return_value->name = g_strdup (trilobite_xml_get_string (node, "NAME"));

	if (return_value->name == NULL) {
		g_free (return_value);
		return NULL;
	}

	return_value->icon = g_strdup (trilobite_xml_get_string (node, "ICON"));
	return_value->date = g_strdup (trilobite_xml_get_string (node, "DATE"));
	return_value->message = g_strdup (trilobite_xml_get_string (node, "MESSAGE"));

	return return_value;

} /* end parse_eazel_news_item */

static UpdateNewsData *
parse_update_news_item (xmlNodePtr node)
{
	UpdateNewsData *return_value;
	return_value = update_news_data_new ();

	return_value->name = g_strdup (trilobite_xml_get_string (node, "NAME"));

	if (return_value->name == NULL) {
		g_free (return_value);
		return NULL;
	}

	return_value->version = g_strdup (trilobite_xml_get_string (node, "VERSION"));
	return_value->priority = g_strdup (trilobite_xml_get_string (node, "PRIORITY"));
	return_value->description = g_strdup (trilobite_xml_get_string (node, "DESCRIPTION"));
	return_value->icon = g_strdup (trilobite_xml_get_string (node, "ICON"));
	return_value->button_label = g_strdup (trilobite_xml_get_string (node, "BUTTON_LABEL"));
	return_value->uri = g_strdup (trilobite_xml_get_string (node, "URI"));
	return_value->softcat_uri = g_strdup (trilobite_xml_get_string (node, "SOFTCAT_URI"));

	return return_value;

} /* end parse_update_news_item */

static GList *
build_services_glist_from_xml (xmlNodePtr node)
{
	GList *return_value;
	xmlNodePtr service;
	ServicesData *sdata;
		
	return_value = NULL;
	service = node->xmlChildrenNode;
	if (service == NULL) {
		return NULL;
	}

	while (service != NULL) {
		sdata = parse_service (service);
		if (sdata != NULL) {
			return_value = g_list_append (return_value, sdata);
		}
		service = service->next;
	}

	return return_value;

} /* end build_services_glist_from_xml */

static GList *
build_eazel_news_glist_from_xml (xmlNodePtr node)
{
	GList *return_value;
	xmlNodePtr news_item;
	EazelNewsData *ndata;

	return_value = NULL;
	news_item = node->xmlChildrenNode;
	if (news_item == NULL) {
		return NULL;
	}

	while (news_item) {
		ndata = parse_eazel_news_item (news_item);
		if (ndata != NULL) {
			return_value = g_list_append (return_value, ndata);
		}
		news_item = news_item->next;
	}

	return return_value;
} /* end build_eazel_news_glist_from_xml */

static GList *
build_update_news_glist_from_xml (xmlNodePtr node)
{
	GList *return_value;
	xmlNodePtr news_item;
	UpdateNewsData *ndata;

	return_value = NULL;
	news_item = node->xmlChildrenNode;
	if (news_item == NULL) {
		return NULL;
	}

	while (news_item) {
		ndata = parse_update_news_item (news_item);
		if (ndata != NULL) {
			return_value = g_list_append (return_value, ndata);
		}
		news_item = news_item->next;
	}

	return return_value;

} /* end build_update_news_glist_from_xml */




static SummaryData *
eazel_summary_data_parse_xml (char *body,
			      int   length)
{
	SummaryData	*return_value;
	xmlDocPtr	doc;
	xmlNodePtr	base;
	xmlNodePtr	child;

	/* <rant> libxml will have a temper tantrum if there is whitespace before the
	*          * first tag.  so we must babysit it.
	*                   */
	while ((length > 0) && isspace (*body)) {
		body++, length--;
	}

	body[length] = '\0';

	doc = xmlParseMemory (body, length);
	if (doc == NULL) {
		return NULL;
	}

	base = doc->root;

	if (base == NULL || 
	    g_strcasecmp (base->name, "SUMMARY_DATA") != 0) {
		xmlFreeDoc (doc);
		return NULL;
	}
	
	return_value = summary_data_new ();
	
	child = base->xmlChildrenNode;
	
	if (child == NULL) {
		xmlFreeDoc (doc);
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
}


struct EazelSummaryFetchHandle {
	TrilobiteReadFileHandle *handle;
	EazelSummaryFetchCallback callback;
	gpointer callback_data;
};



static void
summary_data_fetch_callback (GnomeVFSResult    result,
			     GnomeVFSFileSize  file_size,
			     char             *file_contents,
			     gpointer          callback_data)
{
	EazelSummaryFetchHandle *handle;
	SummaryData *summary_data;

	summary_data = NULL; 
	handle = callback_data;

	if (result == GNOME_VFS_OK) {
		summary_data = eazel_summary_data_parse_xml (file_contents, 
							     file_size);
	}
	
	(*handle->callback) (result, summary_data, handle->callback_data);
	g_free (handle);
	g_free (file_contents);
}


EazelSummaryFetchHandle *
eazel_summary_fetch_data_async (const char *uri,
				EazelSummaryFetchCallback callback,
				gpointer callback_data)
{
	EazelSummaryFetchHandle *handle;

	handle = g_new0 (EazelSummaryFetchHandle, 1);

	handle->callback = callback;
	handle->callback_data = callback_data;

	handle->handle = trilobite_read_entire_file_async (uri, summary_data_fetch_callback, handle);

	return handle;
}


void
eazel_summary_fetch_data_cancel (EazelSummaryFetchHandle *handle)
{
	trilobite_read_file_cancel (handle->handle);
	g_free (handle);
}

