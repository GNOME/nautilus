/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-glib-extensions.h - interface for new functions that conceptually
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

#ifndef NAUTILUS_GLIB_EXTENSIONS_H
#define NAUTILUS_GLIB_EXTENSIONS_H

#include <time.h>
#include <glib.h>

/* Use this until we can switch to G_N_ELEMENTS. */
#define NAUTILUS_N_ELEMENTS(array) (sizeof (array) / sizeof ((array)[0]))

/* Date & time functions. */
GDate *  nautilus_g_date_new_tm             (struct tm   *time_pieces);
char *   nautilus_strdup_strftime           (const char  *format,
					     struct tm   *time_pieces);

/* List functions. */
gboolean nautilus_g_list_exactly_one_item   (GList       *list);
gboolean nautilus_g_list_more_than_one_item (GList       *list);
gboolean nautilus_g_list_equal              (GList       *list_a,
					     GList       *list_b);
GList *  nautilus_g_list_copy		    (const GList *list);
void     nautilus_g_list_free_deep          (GList       *list);
void     nautilus_g_list_safe_for_each      (GList       *list,
					     GFunc        function,
					     gpointer     user_data);

/* NULL terminated string arrays (strv). */
int      nautilus_g_strv_find               (char       **strv,
					     const char  *find_me);

/* return the time in microseconds since the machine was started */
gint64	 nautilus_get_system_time	    (void);

#endif /* NAUTILUS_GLIB_EXTENSIONS_H */
