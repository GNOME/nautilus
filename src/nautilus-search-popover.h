/*
 * SPDX-FileCopyrightText: 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-types.h"

#include <glib.h>
#include <gtk/gtk.h>

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

void
nautilus_search_popover_set_date_range (NautilusSearchPopover *popover,
                                        GPtrArray             *date_range);
void                 nautilus_search_popover_reset_date_range    (NautilusSearchPopover *popover);
void                 nautilus_search_popover_reset_mime_types    (NautilusSearchPopover *popover);

void                 nautilus_search_popover_set_fts_available   (NautilusSearchPopover *popover,
                                                                  gboolean               sensitive);

G_END_DECLS
