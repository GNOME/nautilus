/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-navigation-bar.h - Abstract navigation bar class

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

   Author: Maciej Stachowiak <mjs@eazel.com>
*/

#ifndef NAUTILUS_NAVIGATION_BAR_H
#define NAUTILUS_NAVIGATION_BAR_H

#include <libnautilus-extensions/nautilus-generous-bin.h>

#define NAUTILUS_TYPE_NAVIGATION_BAR (nautilus_navigation_bar_get_type ())
#define NAUTILUS_NAVIGATION_BAR(obj) \
	GTK_CHECK_CAST (obj, NAUTILUS_TYPE_NAVIGATION_BAR, NautilusNavigationBar)
#define NAUTILUS_NAVIGATION_BAR_CLASS(klass) \
	GTK_CHECK_CLASS_CAST (klass, NAUTILUS_TYPE_NAVIGATION_BAR, NautilusNavigationBarClass)
#define NAUTILUS_IS_NAVIGATION_BAR(obj) \
	GTK_CHECK_TYPE (obj, NAUTILUS_TYPE_NAVIGATION_BAR)

typedef struct {
	NautilusGenerousBin parent;
} NautilusNavigationBar;

typedef struct {
	NautilusGenerousBinClass parent_class;

	/* signals */
	void         (* location_changed) (NautilusNavigationBar *bar,
					   const char            *location);

	/* virtual methods */
	char *       (* get_location)     (NautilusNavigationBar *bar);
	void         (* set_location)     (NautilusNavigationBar *bar,
					   const char            *location);

} NautilusNavigationBarClass;

GtkType nautilus_navigation_bar_get_type         (void);
char *  nautilus_navigation_bar_get_location     (NautilusNavigationBar *bar);
void    nautilus_navigation_bar_set_location     (NautilusNavigationBar *bar,
						  const char            *location);

/* `protected' function meant to be used by subclasses to emit the `location_changed' signal */
void    nautilus_navigation_bar_location_changed (NautilusNavigationBar *bar);

#endif /* NAUTILUS_NAVIGATION_BAR_H */
