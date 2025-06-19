/*
 * Copyright (C) 2025 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Original Author: Peter Eisenmann <p3732@getgoogleoff.me>
 */
#pragma once

#include <adwaita.h>
#include <glib.h>
#include <gtk/gtk.h>

#define NAUTILUS_TYPE_DATE_ENTRY_ROW nautilus_date_entry_row_get_type ()
G_DECLARE_FINAL_TYPE (NautilusDateEntryRow, nautilus_date_entry_row, NAUTILUS, DATE_ENTRY_ROW, AdwEntryRow)

NautilusDateEntryRow *
nautilus_date_entry_row_new (GDateTime *initial);

GDateTime *
nautilus_date_entry_row_get_date_time (NautilusDateEntryRow *self);

void
nautilus_date_entry_row_set_date_time (NautilusDateEntryRow *self,
                                       GDateTime            *date_time);
