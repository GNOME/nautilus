/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-search-bar.c - Search bar for Nautilus

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
           Rebecca Schulman <rebecka@eazel.com>
*/

#include <config.h>
#include "nautilus-search-bar.h"

#include <libnautilus-extensions/nautilus-gtk-macros.h>

/* FIXME: This adds nothing to NautilusNavigationBar.
 * Perhaps we can remove it.
 */

static void nautilus_search_bar_initialize_class (NautilusSearchBarClass *class);
static void nautilus_search_bar_initialize       (NautilusSearchBar      *bar);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSearchBar, nautilus_search_bar, NAUTILUS_TYPE_NAVIGATION_BAR)

static void
nautilus_search_bar_initialize_class (NautilusSearchBarClass *klass)
{
}

static void
nautilus_search_bar_initialize (NautilusSearchBar *bar)
{
}
