/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-gconf-extensions.h - Stuff to make GConf easier to use in Nautilus.

   Copyright (C) 2000 Eazel, Inc.

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

#ifndef NAUTILUS_GCONF_EXTENSIONS_H
#define NAUTILUS_GCONF_EXTENSIONS_H

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <libgnome/gnome-defs.h>

BEGIN_GNOME_DECLS

GConfClient *nautilus_gconf_client_get_global (void);
gboolean     nautilus_gconf_handle_error      (GError     **error);
void         nautilus_gconf_set_boolean       (const char  *key,
					       gboolean     boolean_value);
gboolean     nautilus_gconf_get_boolean       (const char  *key);
int          nautilus_gconf_get_integer       (const char  *key);
void         nautilus_gconf_set_integer       (const char  *key,
					       int          int_value);
char *       nautilus_gconf_get_string        (const char  *key);
void         nautilus_gconf_set_string        (const char  *key,
					       const char  *string_value);
GList *      nautilus_gconf_get_string_list   (const char  *key);
void         nautilus_gconf_set_string_list   (const char  *key,
					       GList       *string_list_value);
gboolean     nautilus_gconf_is_default        (const char  *key);
gboolean     nautilus_gconf_monitor_directory (const char  *directory);
void         nautilus_gconf_suggest_sync      (void);

END_GNOME_DECLS

#endif /* NAUTILUS_GCONF_EXTENSIONS_H */
