/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-preferences.h - Preference peek/poke/notify object interface.

   Copyright (C) 1999, 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef NAUTILUS_PREFERENCES_H
#define NAUTILUS_PREFERENCES_H

#include <gtk/gtkobject.h>
#include <libgnome/gnome-defs.h>

#include <libnautilus-extensions/nautilus-preference.h>

BEGIN_GNOME_DECLS

/*
 * A callback which you can register to to be notified when a particular
 * preference changes.
 */
typedef void (*NautilusPreferencesCallback) (gpointer                callback_data);

gboolean            nautilus_preferences_add_callback         (const char                   *name,
							       NautilusPreferencesCallback   callback,
							       gpointer                      callback_data);
gboolean            nautilus_preferences_remove_callback      (const char                   *name,
							       NautilusPreferencesCallback   callback,
							       gpointer                      callback_data);
void                nautilus_preferences_set_boolean          (const char                   *name,
							       gboolean                      value);
gboolean            nautilus_preferences_get_boolean          (const char                   *name,
							       gboolean                      default_value);
void                nautilus_preferences_set_enum             (const char                   *name,
							       int                           value);
int                 nautilus_preferences_get_enum             (const char                   *name,
							       int                           default_value);
void                nautilus_preferences_set                  (const char                   *name,
							       const char                   *value);
char *              nautilus_preferences_get                  (const char                   *name,
							       const gchar                  *default_value);
void                nautilus_preferences_shutdown             (void);


BEGIN_GNOME_DECLS

#endif /* NAUTILUS_PREFERENCES_H */
