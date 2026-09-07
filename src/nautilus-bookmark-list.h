
/*
 * SPDX-FileCopyrightText: 1999, 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 */

/* nautilus-bookmark-list.h - interface for centralized list of bookmarks.
 */

#pragma once

#include "nautilus-types.h"

#include <gio/gio.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_BOOKMARK_LIST (nautilus_bookmark_list_get_type())

G_DECLARE_FINAL_TYPE (NautilusBookmarkList, nautilus_bookmark_list, NAUTILUS, BOOKMARK_LIST, GObject)

NautilusBookmarkList *
nautilus_bookmark_list_new (void);

gboolean
nautilus_bookmark_list_contains (NautilusBookmarkList *bookmarks,
                                 GFile                *location);
gboolean
nautilus_bookmark_list_can_bookmark (NautilusBookmarkList *list,
                                     GFile                *location);
NautilusBookmark *
nautilus_bookmark_list_get_bookmark (NautilusBookmarkList *bookmarks,
                                     GFile                *location);
GList *
nautilus_bookmark_list_get_all (NautilusBookmarkList *bookmarks);

void
nautilus_bookmark_list_add (NautilusBookmarkList *bookmarks,
                            GFile                *location,
                            int                   index);
void
nautilus_bookmark_list_move_item (NautilusBookmarkList *bookmarks,
                                  GFile                *location,
                                  guint                 destination);
void
nautilus_bookmark_list_remove (NautilusBookmarkList *bookmarks,
                               GFile                *location);

G_END_DECLS
