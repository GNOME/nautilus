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

#ifndef NAUTILUS_FILE_H_INCLUDED
#define NAUTILUS_FILE_H_INCLUDED

#include <glib-object.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>

#define NAUTILUS_TYPE_FILE (nautilus_file_get_type ())

G_DECLARE_DERIVABLE_TYPE (NautilusFile, nautilus_file, NAUTILUS, FILE, GObject)

typedef void (*NautilusFileInfoCallback) (NautilusFile *file,
                                          GFileInfo    *info,
                                          GError       *error,
                                          gpointer      user_data);
typedef void (*NautilusThumbnailCallback) (NautilusFile *file,
                                           GdkPixbuf    *pixbuf,
                                           gpointer      user_data);

typedef enum
{
    NAUTILUS_FILE_CHANGE_MOVED,
    NAUTILUS_FILE_CHANGE_RENAMED
} NautilusFileChange;

struct _NautilusFileClass
{
    GObjectClass parent_class;

    void (*renamed) (NautilusFile *file,
                     GFile        *new_location);
};

void nautilus_file_query_info    (NautilusFile              *file,
                                  GCancellable              *cancellable,
                                  NautilusFileInfoCallback   callback,
                                  gpointer                   user_data);
void nautilus_file_get_thumbnail (NautilusFile              *file,
                                  NautilusThumbnailCallback  callback,
                                  gpointer                   user_data);

NautilusFile *nautilus_file_get_existing (GFile        *location);
GFile        *nautilus_file_get_location (NautilusFile *file);
NautilusFile *nautilus_file_get_parent   (NautilusFile *file);

/* Overwrites the info if the file exists in cache.
 * Used by NautilusDirectory when enumerating children.
 */
NautilusFile *nautilus_file_new_with_info (GFile     *location,
                                           GFileInfo *info);
NautilusFile *nautilus_file_new           (GFile     *location);

#endif
