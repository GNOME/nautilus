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

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

char *mozilla_events_get_href_for_mouse_event (gpointer mouse_event);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MOZILLA_EVENTS_H */
