
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
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 *         Ettore Perazzoli <ettore@gnu.org>
 */

#pragma once

#include <gtk/gtk.h>

#define NAUTILUS_TYPE_LOCATION_ENTRY nautilus_location_entry_get_type()
G_DECLARE_FINAL_TYPE (NautilusLocationEntry, nautilus_location_entry,
                      NAUTILUS, LOCATION_ENTRY,
                      GtkWidget)

GtkWidget *nautilus_location_entry_get_entry        (NautilusLocationEntry *entry);

GtkWidget *nautilus_location_entry_new              (void);
void       nautilus_location_entry_set_special_text (NautilusLocationEntry *entry,
                                                     const char            *special_text);
void       nautilus_location_entry_set_location     (NautilusLocationEntry *entry,
                                                     GFile                 *location);
