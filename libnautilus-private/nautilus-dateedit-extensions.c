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

#include <stdio.h>
#include <time.h>

#include <libgnomeui/gnome-dateedit.h>

#include "nautilus-dateedit-extensions.h"

char *
eel_gnome_date_edit_get_date_as_string (GnomeDateEdit *dateedit)
{
	struct tm *time_struct;
	time_t selected_time;
	int day, month, year;
	char *date_string;

	selected_time = gnome_date_edit_get_date (dateedit);
	if (selected_time < 0) {
		return NULL;
	}
	time_struct = localtime (&selected_time);
  
	day = time_struct->tm_mday;
	month = time_struct->tm_mon;
	year = time_struct->tm_year;

	date_string = g_strdup_printf ("%d/%d/%d", month + 1, day, year + 1900);
	return date_string;
  
  

}
