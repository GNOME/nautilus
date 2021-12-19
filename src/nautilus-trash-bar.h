/*
 * Copyright (C) 2006 Paolo Borelli <pborelli@katamail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Paolo Borelli <pborelli@katamail.com>
 *
 */

#pragma once

#include "nautilus-files-view.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_TRASH_BAR (nautilus_trash_bar_get_type ())

G_DECLARE_FINAL_TYPE (NautilusTrashBar, nautilus_trash_bar, NAUTILUS, TRASH_BAR, GtkBin)

GtkWidget *nautilus_trash_bar_new (NautilusFilesView *view);

G_END_DECLS
