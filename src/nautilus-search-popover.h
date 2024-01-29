/* nautilus-search-popover.h
 *
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-query.h"

G_BEGIN_DECLS

typedef enum {
  NAUTILUS_SEARCH_FILTER_CONTENT, /* Full text or filename */
  NAUTILUS_SEARCH_FILTER_DATE,    /* When */
  NAUTILUS_SEARCH_FILTER_LAST,    /* Last modified or last used */
  NAUTILUS_SEARCH_FILTER_TYPE     /* What */
} NautilusSearchFilter;

#define NAUTILUS_TYPE_SEARCH_POPOVER (nautilus_search_popover_get_type())

G_DECLARE_FINAL_TYPE (NautilusSearchPopover, nautilus_search_popover, NAUTILUS, SEARCH_POPOVER, GtkPopover)

GtkWidget*           nautilus_search_popover_new                 (void);

NautilusQuery*       nautilus_search_popover_get_query           (NautilusSearchPopover *popover);

void                 nautilus_search_popover_set_query           (NautilusSearchPopover *popover,
                                                                  NautilusQuery         *query);
void                 nautilus_search_popover_reset_date_range    (NautilusSearchPopover *popover);
void                 nautilus_search_popover_reset_mime_types    (NautilusSearchPopover *popover);

gboolean             nautilus_search_popover_get_fts_enabled     (NautilusSearchPopover *popover);
void                 nautilus_search_popover_set_fts_sensitive   (NautilusSearchPopover *popover,
                                                                  gboolean               sensitive);

G_END_DECLS
