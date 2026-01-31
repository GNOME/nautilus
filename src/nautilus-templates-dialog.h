/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2022 Ignacy Kuchciński
 *
 * Authors: Ignacy Kuchciński <ignacykuchcinski@gmail.com>
 */

#pragma once

#include <adwaita.h>
#include "nautilus-types.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_TEMPLATES_DIALOG (nautilus_templates_dialog_get_type ())

G_DECLARE_FINAL_TYPE (NautilusTemplatesDialog, nautilus_templates_dialog,
                      NAUTILUS, TEMPLATES_DIALOG, AdwDialog)

void nautilus_templates_dialog_new (GtkWindow *parent_window);

NautilusFile * nautilus_templates_dialog_get_selected_file (NautilusTemplatesDialog *self);

G_END_DECLS
