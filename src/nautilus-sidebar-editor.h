/*
 * Copyright (C) 2024 GNOME Foundation Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Original Author: António Fernandes <antoniof@gnome.org>
 */

#pragma once

#include <adwaita.h>

#define NAUTILUS_TYPE_SIDEBAR_EDITOR nautilus_sidebar_editor_get_type ()
G_DECLARE_FINAL_TYPE (NautilusSidebarEditor, nautilus_sidebar_editor,
                      NAUTILUS, SIDEBAR_EDITOR,
                      AdwPreferencesDialog)
