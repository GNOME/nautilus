/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Rebecca Schulman <rebecka@eazel.com>
 */

/* nautilus-simple-search-bar.h - search input containing 
 * a single text entry box
 */

#ifndef NAUTILUS_SIMPLE_SEARCH_BAR_H
#define NAUTILUS_SIMPLE_SEARCH_BAR_H

#include "nautilus-search-bar.h"
#include "nautilus-window.h"
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>


#define NAUTILUS_TYPE_SIMPLE_SEARCH_BAR (nautilus_simple_search_bar_get_type ())
#define NAUTILUS_SIMPLE_SEARCH_BAR(obj) \
	GTK_CHECK_CAST (obj, NAUTILUS_TYPE_SIMPLE_SEARCH_BAR, NautilusSimpleSearchBar)
#define NAUTILUS_SIMPLE_SEARCH_BAR_CLASS(klass) \
	GTK_CHECK_CLASS_CAST (klass, NAUTILUS_TYPE_SIMPLE_SEARCH_BAR, NautilusSimpleSearchBarClass)
#define NAUTILUS_IS_SIMPLE_SEARCH_BAR(obj) \
	GTK_CHECK_TYPE (obj, NAUTILUS_TYPE_SIMPLE_SEARCH_BAR)

typedef struct NautilusSimpleSearchBarDetails  NautilusSimpleSearchBarDetails;

typedef struct NautilusSimpleSearchBar {
	NautilusSearchBar parent;
	NautilusSimpleSearchBarDetails *details;
} NautilusSimpleSearchBar;

typedef struct {
	NautilusSearchBarClass parent_class;
} NautilusSimpleSearchBarClass;

GtkType    nautilus_simple_search_bar_get_type     (void);
GtkWidget* nautilus_simple_search_bar_new          (NautilusWindow *window);

#endif /* NAUTILUS_SIMPLE_SEARCH_BAR_H */
