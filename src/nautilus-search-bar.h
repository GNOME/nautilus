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
 * Authors: Maciej Stachowiak <mjs@eazel.com>
 *          Rebecca Schulman <rebecka@eazel.com>
 */

/* nautilus-search-bar.h - Search bar for Nautilus
 */

#ifndef NAUTILUS_SEARCH_BAR_H
#define NAUTILUS_SEARCH_BAR_H

#include "nautilus-navigation-bar.h"

#include <gtk/gtkhbox.h>
#include <gtk/gtkentry.h>
#include <gtk/gtklabel.h>

#include <libnautilus-private/nautilus-global-preferences.h>

#define NAUTILUS_TYPE_SEARCH_BAR (nautilus_search_bar_get_type ())
#define NAUTILUS_SEARCH_BAR(obj) \
	GTK_CHECK_CAST (obj, NAUTILUS_TYPE_SEARCH_BAR, NautilusSearchBar)
#define NAUTILUS_SEARCH_BAR_CLASS(klass) \
	GTK_CHECK_CLASS_CAST (klass, NAUTILUS_TYPE_SEARCH_BAR, NautilusSearchBarClass)
#define NAUTILUS_IS_SEARCH_BAR(obj) \
	GTK_CHECK_TYPE (obj, NAUTILUS_TYPE_SEARCH_BAR)

typedef struct {
	NautilusNavigationBar parent;
} NautilusSearchBar;

typedef struct {
	NautilusNavigationBarClass parent_class;
} NautilusSearchBarClass;

GtkType nautilus_search_bar_get_type (void);

#endif /* NAUTILUS_SEARCH_BAR_H */
