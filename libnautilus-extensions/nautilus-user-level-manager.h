/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-user-level-manager.h - User level manager interface.

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

#ifndef NAUTILUS_USER_LEVEL_MANAGER_H
#define NAUTILUS_USER_LEVEL_MANAGER_H

#include <gtk/gtkobject.h>
#include <libgnome/gnome-defs.h>
#include <libnautilus-extensions/nautilus-string-list.h>
#include <libnautilus-extensions/nautilus-preference.h>

BEGIN_GNOME_DECLS

typedef struct _NautilusUserLevelManager NautilusUserLevelManager;

/* There is a single NautilusUserLevelManager object.
 * The only thing you need it for is to connect to its signals.
 *
 * "user_level_changed", no parameters
 * 
 */
NautilusUserLevelManager*    nautilus_user_level_manager_get                    (void);
void                         nautilus_user_level_manager_set_user_level         (guint       user_level);
guint                        nautilus_user_level_manager_get_user_level         (void);
guint                        nautilus_user_level_manager_get_num_user_levels    (void);
NautilusStringList          *nautilus_user_level_manager_get_user_level_names   (void);
char			    *nautilus_user_level_manager_make_current_gconf_key (const char *preference_name);
char			    *nautilus_user_level_manager_make_gconf_key         (const char *preference_name,
										 guint         user_level);
char			    *nautilus_user_level_manager_get_user_level_as_string  (void);
void			     nautilus_user_level_manager_set_default_value_if_needed (const char		*preference_name,
										      NautilusPreferenceType       type,
										      guint			user_level,
										      gconstpointer		default_value);
gboolean		     nautilus_user_level_manager_compare_preference_between_user_levels (const char *preference_name,
												 guint	 user_level_a,
												 guint	 user_level_b);

BEGIN_GNOME_DECLS

#endif /* NAUTILUS_USER_LEVEL_MANAGER_H */
