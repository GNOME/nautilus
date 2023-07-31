
/* nautilus-mime-actions.h - uri-specific versions of mime action functions

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Authors: Maciej Stachowiak <mjs@eazel.com>
*/

#pragma once

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "nautilus-types.h"

NautilusFileAttributes nautilus_mime_actions_get_required_file_attributes (void);

GAppInfo *             nautilus_mime_get_default_application_for_file     (NautilusFile            *file);

GAppInfo *             nautilus_mime_get_default_application_for_files    (GList                   *files);

gboolean               nautilus_mime_file_extracts                        (NautilusFile            *file);
gboolean               nautilus_mime_file_opens_in_external_app           (NautilusFile            *file);
gboolean               nautilus_mime_file_launches                        (NautilusFile            *file);
void                   nautilus_mime_activate_files                       (GtkWindow               *parent_window,
									   NautilusWindowSlot *slot,
									   GList              *files,
									   const char         *launch_directory,
									   NautilusOpenFlags   flags,
									   gboolean            user_confirmation);
void                   nautilus_mime_activate_file                        (GtkWindow               *parent_window,
									   NautilusWindowSlot *slot_info,
									   NautilusFile       *file,
									   const char         *launch_directory,
									   NautilusOpenFlags   flags);
guint                  nautilus_mime_types_get_number_of_groups           (void);
const gchar*           nautilus_mime_types_group_get_name                 (guint                    group_index);
GPtrArray*             nautilus_mime_types_group_get_mimetypes            (guint                    group_index);
