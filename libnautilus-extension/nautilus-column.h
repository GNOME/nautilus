/*
 *  nautilus-column.h - Info columns exported by 
 *                      NautilusColumnProvider objects.
 *
 *  Copyright (C) 2003 Novell, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 * 
 *  Author:  Dave Camp <dave@ximian.com>
 *
 */

#pragma once

#include <glib-object.h>
/* This should be removed at some point. */
#include "nautilus-extension-types.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_COLUMN (nautilus_column_get_type())

G_DECLARE_FINAL_TYPE (NautilusColumn, nautilus_column, NAUTILUS, COLUMN, GObject)

NautilusColumn *nautilus_column_new  (const char *name,
                                      const char *attribute,
                                      const char *label,
                                      const char *description);

/* NautilusColumn has the following properties:
 *   name (string)        - the identifier for the column
 *   attribute (string)   - the file attribute to be displayed in the 
 *                          column
 *   label (string)       - the user-visible label for the column
 *   description (string) - a user-visible description of the column
 *   xalign (float)       - x-alignment of the column 
 *   default-sort-order (GtkSortType) - default sort order of the column
 */

G_END_DECLS
