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
 * along with Nautilus.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NAUTILUS_FILE_H_INCLUDED
#define NAUTILUS_FILE_H_INCLUDED

#include <glib-object.h>

#include <gio/gio.h>

#define NAUTILUS_TYPE_FILE (nautilus_file_get_type ())

G_DECLARE_DERIVABLE_TYPE (NautilusFile, nautilus_file, NAUTILUS, FILE, GObject)

typedef void (*NautilusFileInfoCallback) (NautilusFile *file,
                                          GFileInfo    *info,
                                          GError       *error,
                                          gpointer      user_data);

struct _NautilusFileClass
{
    GObjectClass parent_class;
};

void nautilus_file_query_info (NautilusFile             *file,
                               GCancellable             *cancellable,
                               NautilusFileInfoCallback  callback,
                               gpointer                  user_data);

NautilusFile *nautilus_file_new (GFile *location);

#endif
