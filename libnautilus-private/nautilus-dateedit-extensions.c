/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-dateedit-extensions.c -- Extension functions to the gnome-dateedit
   widget 

   Copyright (C) 2000 Eazel, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; see the file COPYING.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Rebecca Schulman <rebecka@eazel.com>
*/

#include <config.h>
#include "nautilus-dateedit-extensions.h"

#include <eel/eel-glib-extensions.h>

char *
nautilus_gnome_date_edit_get_date_as_string (GnomeDateEdit *dateedit)
{
	time_t selected_time;

	selected_time = gnome_date_edit_get_date (dateedit);
	if (selected_time < 0) {
		return NULL;
	}

	/* FIXME: Why doesn't this need to be localized? */
	return eel_strdup_strftime ("%-m/%-d/%Y", localtime (&selected_time));
}
