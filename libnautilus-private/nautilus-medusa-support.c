/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-medusa-support.c - Covers for access to medusa
   from nautilus

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

   Authors: Ramiro Estrugo <ramiro@eazel.com>
            Rebecca Schulman <rebecka@eazel.com>
*/

#include <config.h>
#include <glib.h>
#include <string.h>

#include "nautilus-medusa-support.h"

#ifdef HAVE_MEDUSA
#include <libmedusa/medusa-system-state.h>
#endif HAVE_MEDUSA

gboolean
nautilus_medusa_services_have_been_enabled_by_user (void)
{
#ifdef HAVE_MEDUSA
	return medusa_system_services_have_been_enabled_by_user (g_get_user_name ());
#else
	return FALSE;
#endif
}

gboolean
nautilus_medusa_blocked (void)
{
#ifdef HAVE_MEDUSA
	return medusa_system_services_are_blocked ();
#else
	return TRUE;
#endif
}

void
nautilus_medusa_enable_services (gboolean enable)
{
#ifdef HAVE_MEDUSA
	medusa_enable_medusa_services (enable);
#endif
}

void
nautilus_medusa_add_system_state_changed_callback (NautilusMedusaChangedCallback callback,
						   gpointer callback_data)
{
#ifdef HAVE_MEDUSA
	medusa_execute_when_system_state_changes (MEDUSA_SYSTEM_STATE_ENABLED
						  | MEDUSA_SYSTEM_STATE_DISABLED
						  | MEDUSA_SYSTEM_STATE_BLOCKED,
						  callback,
						  callback_data);
#endif
}
