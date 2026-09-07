
/*
 * SPDX-FileCopyrightText: 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Darin Adler <darin@bentspoon.com>
 */

#pragma once

#include "nautilus-types.h"

#include <adwaita.h>
#include <gtk/gtk.h>

void
nautilus_properties_present_dialog (NautilusFileList *files,
                                    GtkWidget        *parent_widget,
                                    GFile            *current_view_location);

GtkWindow *
nautilus_properties_present_window (NautilusFileList *files,
                                    const char       *startup_id);
