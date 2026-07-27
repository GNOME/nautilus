/* SPDX-Copyright-Text: 2026 The Files contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Peter Eisenmann <p3732@getgoogleoff.me>
 */

#pragma once

#include <gio/gio.h>
#include <glib.h>

G_BEGIN_DECLS

/* Note: Also called for directories */
typedef void (*IterationFileCallback) (GFileInfo *info,
                                       GFile     *location,
                                       gpointer   user_data);

gboolean
nautilus_iterate_directory_recursive (GFile                *directory,
                                      guint                 max_depth,
                                      const char           *attributes,
                                      gboolean              local_only,
                                      GCancellable         *cancellable,
                                      IterationFileCallback file_callback,
                                      GAsyncReadyCallback   done_callback,
                                      gpointer              user_data);

G_END_DECLS