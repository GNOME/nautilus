/* Copyright (C) 2017 Ernestas Kulik <ernestask@gnome.org>
 *
 * This file is part of Nautilus.
 *
 * Nautilus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nautilus.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef NAUTILUS_FILE_TABLE_H_INCLUDED
#define NAUTILUS_FILE_TABLE_H_INCLUDED

#include "nautilus-file.h"

#include <gio/gio.h>

gboolean nautilus_file_table_insert (GFile        *location,
                                     NautilusFile *instance);
gboolean nautilus_file_table_remove (GFile        *location);

NautilusFile *nautilus_file_table_lookup (GFile *location);

#endif
