
/*
 * nautilus-bookmark.h - implementation of individual bookmarks.
 *
 * SPDX-FileCopyrightText: 1999, 2000 Eazel, Inc.
 * SPDX-FileCopyrightText: 2011, Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
