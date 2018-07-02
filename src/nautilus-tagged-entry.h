/* GTK+ 4 implementation of GdTaggedEntry for Nautilus
 * © 2018  Ernestas Kulik <ernestask@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _NautilusTaggedEntryTag NautilusTaggedEntryTag;

#define NAUTILUS_TYPE_TAGGED_ENTRY (nautilus_tagged_entry_get_type ())

G_DECLARE_FINAL_TYPE (NautilusTaggedEntry, nautilus_tagged_entry,
                      NAUTILUS, TAGGED_ENTRY,
                      GtkSearchEntry)

void       nautilus_tagged_entry_add_tag          (NautilusTaggedEntry    *entry,
                                                   NautilusTaggedEntryTag *tag);
void       nautilus_tagged_entry_remove_tag       (NautilusTaggedEntry    *entry,
                                                   NautilusTaggedEntryTag *tag);

GtkWidget *nautilus_tagged_entry_new              (void);

G_END_DECLS
