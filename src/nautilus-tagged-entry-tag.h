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
#include <stdbool.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_TAGGED_ENTRY_TAG (nautilus_tagged_entry_tag_get_type ())

G_DECLARE_FINAL_TYPE (NautilusTaggedEntryTag, nautilus_tagged_entry_tag,
                      NAUTILUS, TAGGED_ENTRY_TAG,
                      GtkWidget)

void       nautilus_tagged_entry_tag_set_label             (NautilusTaggedEntryTag *tag,
                                                            const char             *label);
void       nautilus_tagged_entry_tag_set_show_close_button (NautilusTaggedEntryTag *tag,
                                                            bool                    show_close_button);

GtkWidget *nautilus_tagged_entry_tag_new                   (const char             *label);

G_END_DECLS
