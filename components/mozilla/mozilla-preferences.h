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
 * mozilla-preferences.h - A small C wrapper for poking mozilla preferences
 */

#ifndef MOZILLA_PREFERENCES_H
#define MOZILLA_PREFERENCES_H

#include <glib.h>
#include <gconf/gconf.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

gboolean mozilla_preference_set                 (const char  *preference_name,
						 const char  *new_value);
gboolean mozilla_preference_set_boolean         (const char  *preference_name,
						 gboolean     new_boolean_value);
gboolean mozilla_preference_set_int             (const char  *preference_name,
						 gint         new_int_value);


/* Handle a gconf error.  Post an error dialog only the first time an error occurs.
 * Return TRUE if there was an error.  FALSE if there was no error.
 */
gboolean mozilla_gconf_handle_gconf_error       (GError     **error);


/* Listen for proxy changes on the gconf end of things and route the changes to
 * the mozilla universe.
 */
void     mozilla_gconf_listen_for_proxy_changes (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MOZILLA_PREFERENCES_H */
