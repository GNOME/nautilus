/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-gconf-extensions.h - Stuff to make GConf easier to use.

   Copyright (C) 2000, 2001 Eazel, Inc.

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

#ifndef EEL_GCONF_EXTENSIONS_H
#define EEL_GCONF_EXTENSIONS_H

#include <glib.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

G_BEGIN_DECLS

#define EEL_GCONF_UNDEFINED_CONNECTION 0

GConfClient *eel_gconf_client_get_global     (void);
gboolean     eel_gconf_handle_error          (GError                **error);
void         eel_gconf_set_boolean           (const char             *key,
					      gboolean                boolean_value);
gboolean     eel_gconf_get_boolean           (const char             *key);
int          eel_gconf_get_integer           (const char             *key);
void         eel_gconf_set_integer           (const char             *key,
					      int                     int_value);
char *       eel_gconf_get_string            (const char             *key);
void         eel_gconf_set_string            (const char             *key,
					      const char             *string_value);
GSList *     eel_gconf_get_string_list       (const char             *key);
void         eel_gconf_set_string_list       (const char             *key,
					      const GSList           *string_list_value);
void         eel_gconf_unset                 (const char             *key);
gboolean     eel_gconf_key_is_writable       (const char             *key);
gboolean     eel_gconf_is_default            (const char             *key);
gboolean     eel_gconf_monitor_add           (const char             *directory);
gboolean     eel_gconf_monitor_remove        (const char             *directory);
void         eel_gconf_preload_cache         (const char             *directory,
					      GConfClientPreloadType  preload_type);
void         eel_gconf_suggest_sync          (void);
GConfValue*  eel_gconf_get_value             (const char             *key);
GConfValue*  eel_gconf_get_default_value     (const char             *key);
gboolean     eel_gconf_value_is_equal        (const GConfValue       *a,
					      const GConfValue       *b);
void         eel_gconf_value_free            (GConfValue             *value);
guint        eel_gconf_notification_add      (const char             *key,
					      GConfClientNotifyFunc   notification_callback,
					      gpointer                callback_data);
void         eel_gconf_notification_remove   (guint                   notification_id);
GSList *     eel_gconf_value_get_string_list (const GConfValue       *value);
void         eel_gconf_value_set_string_list (GConfValue             *value,
					      const GSList           *string_list);

G_END_DECLS

#endif /* EEL_GCONF_EXTENSIONS_H */
