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

/* nautilus-search-bar.c - Search bar for Nautilus
 */

#include <config.h>
#include "nautilus-search-bar.h"

#include <eel/eel-gtk-macros.h>

/* FIXME bugzilla.gnome.org 42516: This adds nothing to NautilusNavigationBar.
 * Perhaps we can remove it.
 */

static void nautilus_search_bar_initialize_class (NautilusSearchBarClass *class);
static void nautilus_search_bar_initialize       (NautilusSearchBar      *bar);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusSearchBar, nautilus_search_bar, NAUTILUS_TYPE_NAVIGATION_BAR)

static void
nautilus_search_bar_initialize_class (NautilusSearchBarClass *klass)
{
}

static void
nautilus_search_bar_initialize (NautilusSearchBar *bar)
{
}
