/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-glib-extensions.c - implementation of new functions that conceptually
                                belong in glib. Perhaps some of these will be
                                actually rolled into glib someday.

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

   Authors: John Sullivan <sullivan@eazel.com>
*/

#include "nautilus-glib-extensions.h"

#include "nautilus-lib-self-check-functions.h"


/**
 * nautilus_g_date_new_tm:
 * 
 * Get a new GDate * for the date represented by a tm struct. 
 * The caller is responsible for g_free-ing the result.
 * @time_pieces: Pointer to a tm struct representing the date to be converted.
 * 
 * Returns: Newly allocated date.
 * 
 **/
GDate *
nautilus_g_date_new_tm (struct tm *time_pieces)
{
	/* tm uses 0-based months; GDate uses 1-based months.
	 * tm_year needs 1900 added to get the full year.
	 */
	return g_date_new_dmy (time_pieces->tm_mday,
			       time_pieces->tm_mon + 1,
			       time_pieces->tm_year + 1900);
}

/**
 * nautilus_strdup_strftime:
 *
 * Cover for standard date-and-time-formatting routine strftime that returns
 * a newly-allocated string of the correct size. The caller is responsible
 * for g_free-ing the returned string.
 * @format: format string to pass to strftime. See strftime documentation
 * for details.
 * @time_pieces: date/time, in struct format.
 * 
 * Return value: Newly allocated string containing the formatted time.
 **/
char *
nautilus_strdup_strftime (const char *format, struct tm *time_pieces)
{
	char *result;
	size_t string_length;

	string_length = strftime (NULL, UINT_MAX, format, time_pieces);
	result = g_malloc (string_length + 1);
	strftime (result, string_length + 1, format, time_pieces);

	return result;
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

static void 
check_tm_to_g_date (time_t time)
{
	struct tm *before_conversion;
	struct tm *after_conversion;
	GDate *date;

	before_conversion = localtime (&time);
	date = nautilus_g_date_new_tm (before_conversion);

	after_conversion = g_new0 (struct tm, 1);
	g_date_to_struct_tm (date, after_conversion);

	NAUTILUS_CHECK_INTEGER_RESULT (after_conversion->tm_mday,
				       before_conversion->tm_mday);
	NAUTILUS_CHECK_INTEGER_RESULT (after_conversion->tm_mon,
				       before_conversion->tm_mon);
	NAUTILUS_CHECK_INTEGER_RESULT (after_conversion->tm_year,
				       before_conversion->tm_year);
	
	g_free (after_conversion);
}

void
nautilus_self_check_glib_extensions (void)
{
	check_tm_to_g_date (0);			/* lower limit */
	check_tm_to_g_date ((time_t) -1);	/* upper limit */
	check_tm_to_g_date (time (NULL));	/* current time */
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */

