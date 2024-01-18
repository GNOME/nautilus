
/*
 * Nautilus
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 */

/* nautilus-bookmark-list.h - interface for centralized list of bookmarks.
 */

#pragma once

#include "nautilus-bookmark.h"
#include <gio/gio.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_BOOKMARK_LIST (nautilus_bookmark_list_get_type())

G_DECLARE_FINAL_TYPE (NautilusBookmarkList, nautilus_bookmark_list, NAUTILUS, BOOKMARK_LIST, GObject)

NautilusBookmarkList *  nautilus_bookmark_list_new                 (void);
void                    nautilus_bookmark_list_append              (NautilusBookmarkList   *bookmarks,
								    NautilusBookmark *bookmark);
NautilusBookmark *      nautilus_bookmark_list_item_with_location  (NautilusBookmarkList *bookmarks,
								    GFile                *location,
								    guint                *index);
gboolean                nautilus_bookmark_list_can_bookmark_location (NautilusBookmarkList *list,
								      GFile                *location);
GList *                 nautilus_bookmark_list_get_all             (NautilusBookmarkList   *bookmarks);
gboolean                nautilus_bookmark_list_contains            (NautilusBookmarkList   *bookmarks,
								    NautilusBookmark *bookmark);
void                    nautilus_bookmark_list_delete_item_at      (NautilusBookmarkList   *bookmarks,
								    guint                   index);
void                    nautilus_bookmark_list_delete_items_with_uri (NautilusBookmarkList *bookmarks,
								    const char		   *uri);
void                    nautilus_bookmark_list_insert_item         (NautilusBookmarkList   *bookmarks,
								    NautilusBookmark *bookmark,
								    guint                   index);
guint                   nautilus_bookmark_list_length              (NautilusBookmarkList   *bookmarks);
NautilusBookmark *      nautilus_bookmark_list_item_at             (NautilusBookmarkList   *bookmarks,
								    guint                   index);
void                    nautilus_bookmark_list_move_item           (NautilusBookmarkList *bookmarks,
								    guint                 index,
								    guint                 destination);
G_END_DECLS
