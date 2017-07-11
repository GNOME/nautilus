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

#include "nautilus-directory.h"

G_DEFINE_TYPE (NautilusDirectory, nautilus_directory, NAUTILUS_TYPE_FILE)

static void
nautilus_directory_class_init (NautilusDirectoryClass *klass)
{
}

static void
nautilus_directory_init (NautilusDirectory *self)
{
}

NautilusFile *
nautilus_directory_new (GFile *location)
{
    gpointer instance;

    g_return_val_if_fail (G_IS_FILE (location), NULL);

    instance = g_object_new (NAUTILUS_TYPE_DIRECTORY,
                             "location", location,
                             NULL);

    g_assert (NAUTILUS_IS_DIRECTORY (instance));

    return instance;
}
