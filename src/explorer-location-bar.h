/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* explorer-location-bar.h - Location bar for the GNOME Explorer.

   Copyright (C) 1999, 2000 Free Software Foundation

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

   Author: Ettore Perazzoli <ettore@gnu.org>
*/

#ifndef _EXPLORER_LOCATION_BAR_H
#define _EXPLORER_LOCATION_BAR_H
#include <gnome.h>

#define EXPLORER_LOCATION_BAR(obj) \
  GTK_CHECK_CAST (obj, explorer_location_bar_get_type (), ExplorerLocationBar)
#define EXPLORER_LOCATION_BAR_CLASS(klass) \
  GTK_CHECK_CLASS_CAST (klass, explorer_location_bar_get_type (), ExplorerLocationBarClass)
#define EXPLORER_IS_LOCATION_BAR(obj) \
  GTK_CHECK_TYPE (obj, explorer_location_bar_get_type ())

struct _ExplorerLocationBar {
	GtkHBox hbox;

	GtkWidget *label;
	GtkWidget *entry;
};
typedef struct _ExplorerLocationBar ExplorerLocationBar;

struct _ExplorerLocationBarClass {
	GtkHBoxClass parent_class;
	void         (*location_changed) (ExplorerLocationBar *location_bar,
					  const char *uri_string);
};
typedef struct _ExplorerLocationBarClass ExplorerLocationBarClass;


GtkType		 explorer_location_bar_get_type	 (void);
GtkWidget	*explorer_location_bar_new	 (void);
void		 explorer_location_bar_set_uri_string
						 (ExplorerLocationBar *bar,
						  const gchar *uri_string);

#endif /* _EXPLORER_LOCATION_BAR_H */
