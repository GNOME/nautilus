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

#define NAUTILUS_TYPE_DATE_RANGE_DIALOG nautilus_date_range_dialog_get_type ()
G_DECLARE_FINAL_TYPE (NautilusDateRangeDialog, nautilus_date_range_dialog, NAUTILUS, DATE_RANGE_DIALOG, AdwDialog)

NautilusDateRangeDialog *
nautilus_date_range_dialog_new (GPtrArray *initial_date_range);

GPtrArray *
nautilus_date_range_dialog_get_range (NautilusDateRangeDialog *self);
