
/*
 * SPDX-FileCopyrightText: 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Maciej Stachowiak <mjs@eazel.com>
 */

#pragma once

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "nautilus-types.h"

NautilusAttributes     nautilus_mime_actions_get_required_attributes      (void);

GAppInfo *             nautilus_mime_get_default_application_for_file     (NautilusFile            *file);

GAppInfo *             nautilus_mime_get_default_application_for_files    (GList                   *files);

gboolean               nautilus_mime_is_video                             (const char              *content_type);
gboolean               nautilus_mime_file_extracts                        (NautilusFile            *file);
gboolean               nautilus_mime_file_opens_in_external_app           (NautilusFile            *file);
gboolean               nautilus_mime_file_launches                        (NautilusFile            *file);
void                   nautilus_mime_activate_files                       (GtkWindow               *parent_window,
									   NautilusWindowSlot *slot,
									   GList              *files,
									   const char         *launch_directory,
									   NautilusOpenFlags   flags,
									   gboolean            user_confirmation);
GPtrArray*             nautilus_mime_types_group_get_mimetypes            (guint                    group_index);
