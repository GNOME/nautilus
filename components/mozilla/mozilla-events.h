/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Copyright (C) 2000 Eazel, Inc.
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
 *  Author: Ramiro Estrugo <ramiro@eazel.com>
 *
 */

/*
 * mozilla-event.h - A small C wrapper for grokking mozilla dom events
 */

#ifndef MOZILLA_EVENTS_H
#define MOZILLA_EVENTS_H

#include <glib.h>
#include "gtkmozembed.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

gboolean mozilla_events_is_key_return (gpointer dom_event);

char *mozilla_events_get_href_for_event (gpointer dom_event);

void mozilla_navigate_to_anchor (GtkMozEmbed *mozilla_embed, const char *anchor);

#if 0
gboolean mozilla_events_is_in_form_POST_submit (gpointer dom_event);

gboolean mozilla_events_is_url_in_iframe (GtkMozEmbed *embed, const char *uri);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MOZILLA_EVENTS_H */
