/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-preferences.c - Preference peek/poke/notify interface.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

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

#ifndef EEL_PREFERENCES_H
#define EEL_PREFERENCES_H

#include <glib.h>
#include <eel/eel-gconf-extensions.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/*
 * A callback which you can register to to be notified when a particular
 * preference changes.
 */
typedef void (*EelPreferencesCallback) (gpointer callback_data);

/* Preferences getters and setters */
gboolean       eel_preferences_get_boolean                     (const char              *name);
void           eel_preferences_set_boolean                     (const char              *name,
								gboolean                 boolean_value);
int            eel_preferences_get_integer                     (const char              *name);
void           eel_preferences_set_integer                     (const char              *name,
								int                      int_value);
guint          eel_preferences_get_uint                        (const char              *name);
void           eel_preferences_set_uint                        (const char              *name,
								guint                    uint_value);
guint          eel_preferences_get_enum                        (const char              *name);
void           eel_preferences_set_enum                        (const char              *name,
								guint                   int_value);
char *         eel_preferences_get                             (const char              *name);
void           eel_preferences_set                             (const char              *name,
								const char              *string_value);
char **        eel_preferences_get_string_array                 (const char              *name);
void           eel_preferences_set_string_array                 (const char              *name,
								 char                   **strv_value);

void           eel_preferences_unset                            (const char              *name);

/* Writability of a key */
gboolean       eel_preferences_key_is_writable                 (const char              *name);

/* Callbacks */
void           eel_preferences_add_callback                    (const char              *name,
								EelPreferencesCallback   callback,
								gpointer                 callback_data);
void           eel_preferences_add_callback_while_alive        (const char              *name,
								EelPreferencesCallback   callback,
								gpointer                 callback_data,
								GObject                 *alive_object);
void           eel_preferences_remove_callback                 (const char              *name,
								EelPreferencesCallback   callback,
								gpointer                 callback_data);

/* Variables that are automatically updated (lightweight "callbacks") */
void           eel_preferences_add_auto_string                 (const char              *name,
								const char             **storage);
void           eel_preferences_add_auto_string_array           (const char              *name,
								char                  ***storage);
void           eel_preferences_add_auto_string_array_as_quarks (const char              *name,
								GQuark                 **storage);
void           eel_preferences_add_auto_integer                (const char              *name,
								int                     *storage);
void           eel_preferences_add_auto_enum                   (const char              *name,
								guint                   *storage);
void           eel_preferences_add_auto_boolean                (const char              *name,
								gboolean                *storage);
void           eel_preferences_remove_auto_string              (const char              *name,
								const char             **storage);
void           eel_preferences_remove_auto_string_array        (const char              *name,
								char                  ***storage);
void           eel_preferences_remove_auto_integer             (const char              *name,
								int                     *storage);
void           eel_preferences_remove_auto_boolean             (const char              *name,
								int                     *storage);

/* Preferences attributes */

gboolean       eel_preferences_get_is_invisible                (const char              *name);
void           eel_preferences_set_is_invisible                (const char              *name,
								gboolean                 invisible);
char *         eel_preferences_get_description                 (const char              *name);
void           eel_preferences_set_description                 (const char              *name,
								const char              *description);
char *         eel_preferences_get_enumeration_id              (const char              *name);
void           eel_preferences_set_enumeration_id              (const char              *name,
								const char              *enumeration_id);

void        eel_preferences_set_emergency_fallback_string      (const char    *name,
								const char    *value);
void        eel_preferences_set_emergency_fallback_integer     (const char    *name,
								int            value);
void        eel_preferences_set_emergency_fallback_boolean     (const char    *name,
								gboolean       value);
void        eel_preferences_set_emergency_fallback_string_array(const char    *name,
								char         **value);
GConfValue *eel_preferences_get_emergency_fallback             (const char    *name);


gboolean       eel_preferences_monitor_directory               (const char              *directory);
gboolean       eel_preferences_is_visible                      (const char              *name);
void           eel_preferences_init                      (const char              *storage_path);

void eel_preferences_builder_connect_bool		         (GtkBuilder *builder,
								  const char  *component,
								  const char  *key);
void eel_preferences_builder_connect_bool_slave			 (GtkBuilder *builder,
								  const char  *component,
								  const char  *key);
void eel_preferences_builder_connect_string_enum_combo_box	 (GtkBuilder *builder,
								  const char  *component,
								  const char  *key,
								  const char **values);
void eel_preferences_builder_connect_string_enum_combo_box_slave (GtkBuilder *builder,
								  const char  *component,
								  const char  *key);

void eel_preferences_builder_connect_uint_enum			 (GtkBuilder *builder,
								  const char  *component,
								  const char  *key,
								  const guint *values,
								  int          num_values);
void eel_preferences_builder_connect_string_enum_radio_button	 (GtkBuilder *builder,
								  const char **components,
								  const char  *key,
								  const char **values);
void eel_preferences_builder_connect_list_enum			 (GtkBuilder *builder,
						   		  const char **components,
								  const char  *key,
								  const char **values);


G_END_DECLS

#endif /* EEL_PREFERENCES_H */
