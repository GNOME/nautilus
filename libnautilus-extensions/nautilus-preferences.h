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

#include <nautilus-widgets/nautilus-preference.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_PREFERENCES            (nautilus_preferences_get_type ())
#define NAUTILUS_PREFERENCES(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_PREFERENCES, NautilusPreferences))
#define NAUTILUS_PREFERENCES_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PREFERENCES, NautilusPreferencesClass))
#define NAUTILUS_IS_PREFERENCES(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_PREFERENCES))
#define NAUTILUS_IS_PREFERENCES_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PREFERENCES))

typedef struct NautilusPreferences	    NautilusPreferences;
typedef struct NautilusPreferencesClass     NautilusPreferencesClass;
typedef struct NautilusPreferencesDetails   NautilusPreferencesDetails;

struct NautilusPreferences
{
	GtkObject			object;
	NautilusPreferencesDetails	*details;
};

/*
 * A callback which you can register to to be notified when a particular
 * preference changes.
 */
typedef void (*NautilusPreferencesCallback) (gpointer                callback_data);

struct NautilusPreferencesClass
{
	GtkObjectClass object_class;
};

GtkType              nautilus_preferences_get_type               (void);
GtkObject *          nautilus_preferences_new                    (const char                   *domain);
NautilusPreference * nautilus_preferences_get_preference         (const NautilusPreferences    *preferences,
								  const char                   *name);
void                 nautilus_preferences_set_info               (NautilusPreferences          *preferences,
								  const char                   *name,
								  const char                   *description,
								  NautilusPreferenceType        type,
								  gconstpointer                 default_value);
void                 nautilus_preferences_enum_add_entry         (NautilusPreferences          *preferences,
								  const char                   *name,
								  const char                   *entry_name,
								  const char                   *entry_description,
								  int                           entry_value);
gboolean             nautilus_preferences_add_enum_callback      (NautilusPreferences          *preferences,
								  const char                   *name,
								  NautilusPreferencesCallback   callback,
								  gpointer                      callback_data);
gboolean             nautilus_preferences_add_boolean_callback   (NautilusPreferences          *preferences,
								  const char                   *name,
								  NautilusPreferencesCallback   callback,
								  gpointer                      callback_data);
gboolean             nautilus_preferences_add_callback           (NautilusPreferences          *preferences,
								  const char                   *name,
								  NautilusPreferencesCallback   callback,
								  gpointer                      callback_data);
gboolean             nautilus_preferences_remove_callback        (NautilusPreferences          *preferences,
								  const char                   *name,
								  NautilusPreferencesCallback   callback,
								  gpointer                      callback_data);
void                 nautilus_preferences_set_boolean            (NautilusPreferences          *preferences,
								  const char                   *name,
								  gboolean                      value);
gboolean             nautilus_preferences_get_boolean            (const NautilusPreferences    *preferences,
								  const char                   *name,
								  gboolean                      default_value);
void                 nautilus_preferences_set_enum               (NautilusPreferences          *preferences,
								  const char                   *name,
								  int                           value);
int                  nautilus_preferences_get_enum               (const NautilusPreferences    *preferences,
								  const char                   *name,
								  int                           default_value);
void                 nautilus_preferences_set                    (NautilusPreferences          *preferences,
								  const char                   *name,
								  const char                   *value);
char *               nautilus_preferences_get                    (const NautilusPreferences    *preferences,
								  const char                   *name,
								  const gchar                  *default_value);
NautilusPreferences *nautilus_preferences_get_global_preferences (void);
gboolean             nautilus_preferences_init                   (int                           argc,
								  char                        **argv);

BEGIN_GNOME_DECLS

#endif /* NAUTILUS_PREFERENCES_H */
