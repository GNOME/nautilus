/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-system-preferences.c - Preferences that cannot be managed with gconf

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

#include <config.h>
#include <glib.h>
#include <string.h>

#include "nautilus-global-preferences.h"
#include "nautilus-system-preferences.h"

#ifdef HAVE_MEDUSA
#include <libmedusa/medusa-system-state.h>
#endif HAVE_MEDUSA


gboolean
nautilus_is_system_preference (const char *preference_name)
{
	g_return_val_if_fail (preference_name != NULL, FALSE);

	return strcmp (preference_name, NAUTILUS_PREFERENCES_USE_FAST_SEARCH) == 0;
}

gboolean
nautilus_system_preference_get_boolean (const char *preference_name)
{
	g_return_val_if_fail (preference_name != NULL, FALSE);
	g_return_val_if_fail (nautilus_is_system_preference (preference_name), FALSE);

#ifdef HAVE_MEDUSA
	return medusa_system_services_have_been_enabled_by_user (g_get_user_name ());
#else
	return FALSE;
#endif
}

void
nautilus_system_preference_set_boolean (const char *preference_name,
					gboolean value)
{
	g_return_if_fail (preference_name != NULL);
	g_return_if_fail (nautilus_is_system_preference (preference_name));

#ifdef HAVE_MEDUSA
	medusa_enable_medusa_services (value);
#endif
}


