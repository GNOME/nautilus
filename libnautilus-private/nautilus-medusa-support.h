/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-medusa-support.h - Covers for access to medusa
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

#ifndef NAUTILUS_MEDUSA_SUPPORT_H
#define NAUTILUS_MEDUSA_SUPPORT_H

#include <glib.h>

typedef void (* NautilusMedusaChangedCallback)     (gpointer data);

typedef enum {
	NAUTILUS_CRON_STATUS_ON,
	NAUTILUS_CRON_STATUS_OFF,
	NAUTILUS_CRON_STATUS_UNKNOWN
} NautilusCronStatus;
	

gboolean           nautilus_medusa_services_are_enabled               (void);
NautilusCronStatus nautilus_medusa_check_cron_is_enabled              (void);
char *             nautilus_medusa_get_explanation_of_enabling        (void);

#endif /* NAUTILUS_MEDUSA_SUPPORT_H */

