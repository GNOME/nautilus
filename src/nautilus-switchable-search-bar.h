/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Rebecca Schulman <rebecka@eazel.com> 
 */

/* nautilus-switchable-navigation-bar.h - Navigation bar for Nautilus
 *  that allows switching between the location bar and the search bar
 */


#ifndef NAUTILUS_SWITCHABLE_SEARCH_BAR_H
#define NAUTILUS_SWITCHABLE_SEARCH_BAR_H

#include "nautilus-search-bar.h"
#include "nautilus-window.h"
#include <gtk/gtkhbox.h>

#define NAUTILUS_TYPE_SWITCHABLE_SEARCH_BAR (nautilus_switchable_search_bar_get_type ())
#define NAUTILUS_SWITCHABLE_SEARCH_BAR(obj) \
	GTK_CHECK_CAST (obj, NAUTILUS_TYPE_SWITCHABLE_SEARCH_BAR, NautilusSwitchableSearchBar)
#define NAUTILUS_SWITCHABLE_SEARCH_BAR_CLASS(klass) \
	GTK_CHECK_CLASS_CAST (klass, NAUTILUS_TYPE_SWITCHABLE_SEARCH_BAR, NautilusSwitchableSearchBarClass)
#define NAUTILUS_IS_SWITCHABLE_SEARCH_BAR(obj) \
	GTK_CHECK_TYPE (obj, NAUTILUS_TYPE_SWITCHABLE_SEARCH_BAR)

typedef struct {
	NautilusSearchBar parent_slot;

	NautilusSearchBarMode mode;
	GtkHBox *container;
	GtkWidget *complex_search_bar;
	GtkWidget *simple_search_bar;
} NautilusSwitchableSearchBar;

typedef struct {
	NautilusSearchBarClass parent_slot;

	void (* mode_changed) (NautilusSwitchableSearchBar *search_bar,
			       NautilusSearchBarMode        mode);
} NautilusSwitchableSearchBarClass;

GtkType    nautilus_switchable_search_bar_get_type (void);
GtkWidget *nautilus_switchable_search_bar_new      (NautilusWindow *window);
void       nautilus_switchable_search_bar_set_mode (NautilusSwitchableSearchBar *search_bar,
						    NautilusSearchBarMode        mode);

#endif /* NAUTILUS_SWITCHABLE_SEARCH_BAR_H */
