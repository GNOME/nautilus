/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-system-preferences.h - Preferences that cannot be managed 
   with gconf

   Copyright (C) 2001 Eazel, Inc.

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

   Authors: Rebecca Schulman <rebecka@eazel.com>
*/

#ifndef NAUTILUS_SYSTEM_PREFERENCES_H
#define NAUTILUS_SYSTEM_PREFERENCES_H

#include <glib.h>

gboolean        nautilus_is_system_preference                                  (const char *preference_name);

gboolean        nautilus_system_preference_get_boolean                         (const char *preference_name);
void            nautilus_system_preference_set_boolean                         (const char *preference_name,
										gboolean preference_value);

void            nautilus_system_preferences_initialize                         (void);

/* Set up callbacks that will change system preferences if they are changed at the command line. */
void            nautilus_system_preferences_check_for_system_level_changes     (void);


#endif /* NAUTILUS_SYSTEM_PREFERENCES_H */

