/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Novell, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 */

#include <config.h>
#include "nautilus-query.h"

#include <eel/eel-gtk-macros.h>

struct NautilusQueryDetails {
	char *text;
};

static void  nautilus_query_class_init       (NautilusQueryClass *class);
static void  nautilus_query_init             (NautilusQuery      *query);

G_DEFINE_TYPE (NautilusQuery,
	       nautilus_query,
	       G_TYPE_OBJECT);

static GObjectClass *parent_class = NULL;

static void
finalize (GObject *object)
{
	NautilusQuery *query;

	query = NAUTILUS_QUERY (object);
	
	g_free (query->details->text);
	g_free (query->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
nautilus_query_class_init (NautilusQueryClass *class)
{
	GObjectClass *gobject_class;

	parent_class = g_type_class_peek_parent (class);

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;
}

static void
nautilus_query_init (NautilusQuery *query)
{
	query->details = g_new0 (NautilusQueryDetails, 1);
}

NautilusQuery *
nautilus_query_new (void)
{
	return g_object_new (NAUTILUS_TYPE_QUERY,  NULL);
}


G_CONST_RETURN char *
nautilus_query_get_text (NautilusQuery *query)
{
	return query->details->text;
}

void 
nautilus_query_set_text (NautilusQuery *query, const char *text)
{
	g_free (query->details->text);
	query->details->text = g_strdup (text);
}

char *
nautilus_query_to_readable_string (NautilusQuery *query)
{
	if (!query || !query->details->text) {
		return g_strdup ("Search");
	}

	return g_strdup_printf ("Search for \"%s\"", query->details->text);
}

