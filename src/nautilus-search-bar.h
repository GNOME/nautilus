/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-search-bar.h - Search bar for Nautilus

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

#ifndef NAUTILUS_SEARCH_BAR_H
#define NAUTILUS_SEARCH_BAR_H

#include "nautilus-navigation-bar.h"

#include <gtk/gtkhbox.h>
#include <gtk/gtkentry.h>
#include <gtk/gtklabel.h>

#include <libnautilus-extensions/nautilus-global-preferences.h>

#define NAUTILUS_TYPE_SEARCH_BAR (nautilus_search_bar_get_type ())
#define NAUTILUS_SEARCH_BAR(obj) \
	GTK_CHECK_CAST (obj, NAUTILUS_TYPE_SEARCH_BAR, NautilusSearchBar)
#define NAUTILUS_SEARCH_BAR_CLASS(klass) \
	GTK_CHECK_CLASS_CAST (klass, NAUTILUS_TYPE_SEARCH_BAR, NautilusSearchBarClass)
#define NAUTILUS_IS_SEARCH_BAR(obj) \
	GTK_CHECK_TYPE (obj, NAUTILUS_TYPE_SEARCH_BAR)


typedef struct NautilusSearchBarDetails NautilusSearchBarDetails;

typedef struct NautilusSearchBar {
	NautilusNavigationBar parent;
} NautilusSearchBar;

typedef struct {
	NautilusNavigationBarClass parent_class;

	/* virtual method */
	void (*set_search_controls) (NautilusSearchBar *search_bar,
				     const char *location);

} NautilusSearchBarClass;

GtkType                   nautilus_search_bar_get_type     	         (void);
GtkWidget*                nautilus_search_bar_new          	         (void);


void                      nautilus_search_bar_set_search_controls        (NautilusSearchBar *search_bar,
									  const char *location);
/* FIXME:Do we need a protected location_changed function here too? */
#endif /* NAUTILUS_SEARCH_BAR_H */

