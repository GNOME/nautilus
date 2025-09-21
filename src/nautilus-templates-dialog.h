/* nautilus-templates-dialog.h
 *
 * Copyright 2022 Ignacy Kuchciński <ignacykuchcinski@gmail.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>
#include "nautilus-types.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_TEMPLATES_DIALOG (nautilus_templates_dialog_get_type ())

G_DECLARE_FINAL_TYPE (NautilusTemplatesDialog, nautilus_templates_dialog, NAUTILUS, TEMPLATES_DIALOG, AdwDialog)

void nautilus_templates_dialog_new (GtkWindow *parent_window);

NautilusFile * nautilus_templates_dialog_get_selected_file (NautilusTemplatesDialog *self);

G_END_DECLS
