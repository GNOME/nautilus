
/* nautilus-bookmark.h - implementation of individual bookmarks.
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
 * Copyright (C) 2011, Red Hat, Inc.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 */

#pragma once

#include <gtk/gtk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_BOOKMARK nautilus_bookmark_get_type()

G_DECLARE_FINAL_TYPE (NautilusBookmark, nautilus_bookmark, NAUTILUS, BOOKMARK, GObject)

NautilusBookmark *    nautilus_bookmark_new                    (GFile *location,
                                                                const char *custom_name);
const char *          nautilus_bookmark_get_name               (NautilusBookmark      *bookmark);
GFile *               nautilus_bookmark_get_location           (NautilusBookmark      *bookmark);
char *                nautilus_bookmark_get_uri                (NautilusBookmark      *bookmark);
GIcon *               nautilus_bookmark_get_icon               (NautilusBookmark      *bookmark);
GIcon *               nautilus_bookmark_get_symbolic_icon      (NautilusBookmark      *bookmark);

void                  nautilus_bookmark_take_selected_uris     (NautilusBookmark      *bookmark,
								GStrv                  selected_uris);
GStrv                 nautilus_bookmark_get_selected_uris      (NautilusBookmark      *bookmark);
void                  nautilus_bookmark_set_name               (NautilusBookmark      *bookmark,
                                                                const char            *new_name);

G_END_DECLS
