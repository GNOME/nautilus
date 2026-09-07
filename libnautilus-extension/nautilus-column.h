/*
 * nautilus-column.h - Info columns exported by NautilusColumnProvider objects.
 *
 * SPDX-FileCopyrightText: 2003 Novell, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Dave Camp <dave@ximian.com>
 */

#pragma once

#if !defined (NAUTILUS_EXTENSION_H) && !defined (NAUTILUS_COMPILATION)
#warning "Only <nautilus-extension.h> should be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_COLUMN (nautilus_column_get_type())

G_DECLARE_FINAL_TYPE (NautilusColumn, nautilus_column, NAUTILUS, COLUMN, GObject)

NautilusColumn *nautilus_column_new  (const char *name,
                                      const char *attribute,
                                      const char *label,
                                      const char *description);

G_END_DECLS
