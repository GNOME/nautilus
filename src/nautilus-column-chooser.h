
/*
 * SPDX-FileCopyrightText: 2004 Novell, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Dave Camp <dave@ximian.com>
 */

#pragma once

#include "nautilus-types.h"

#include <adwaita.h>
#include <gtk/gtk.h>

#define NAUTILUS_TYPE_COLUMN_CHOOSER nautilus_column_chooser_get_type()

G_DECLARE_FINAL_TYPE (NautilusColumnChooser, nautilus_column_chooser, NAUTILUS, COLUMN_CHOOSER, AdwDialog);

GtkWidget *nautilus_column_chooser_new             (NautilusFile            *file);
