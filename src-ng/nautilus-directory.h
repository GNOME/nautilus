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

#ifndef NAUTILUS_DIRECTORY_H_INCLUDED
#define NAUTILUS_DIRECTORY_H_INCLUDED

#include "nautilus-file.h"

#define NAUTILUS_TYPE_DIRECTORY (nautilus_directory_get_type ())

G_DECLARE_DERIVABLE_TYPE (NautilusDirectory, nautilus_directory,
                          NAUTILUS, DIRECTORY,
                          NautilusFile)

typedef void (*NautilusEnumerateChildrenCallback) (NautilusDirectory *directory,
                                                   GList             *children,
                                                   GError            *error,
                                                   gpointer           user_data);

struct _NautilusDirectoryClass
{
    NautilusFileClass parent_class;
};

void nautilus_directory_enumerate_children (NautilusDirectory                 *directory,
                                            GCancellable                      *cancellable,
                                            NautilusEnumerateChildrenCallback  callback,
                                            gpointer                           user_data);

NautilusFile *nautilus_directory_new (GFile *location);

#endif
