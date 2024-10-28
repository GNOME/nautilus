/*
 * Copyright (C) 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_APP_CHOOSER (nautilus_app_chooser_get_type())

G_DECLARE_FINAL_TYPE (NautilusAppChooser, nautilus_app_chooser, NAUTILUS, APP_CHOOSER, AdwDialog)

NautilusAppChooser *nautilus_app_chooser_new (GList     *files);

GAppInfo           *nautilus_app_chooser_get_app_info (NautilusAppChooser *self);

G_END_DECLS
