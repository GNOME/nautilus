
/*
 * SPDX-FileCopyrightText: 2004 Novell, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Dave Camp <dave@ximian.com>
 */

#pragma once

#include "nautilus-types.h"

#include <glib.h>

GList *nautilus_get_all_columns       (void);
GList *nautilus_get_common_columns    (void);
GList *nautilus_get_columns_for_file (NautilusFile *file);
GList *nautilus_column_list_copy      (GList       *columns);
void   nautilus_column_list_free      (GList       *columns);

GList *nautilus_sort_columns          (GList       *columns,
				       char       **column_order);
void   nautilus_column_save_metadata  (NautilusFile *file,
                                       GStrv         column_order,
                                       GStrv         visible_column);

GStrv  nautilus_column_get_default_visible_columns (NautilusFile *file);
GStrv  nautilus_column_get_visible_columns         (NautilusFile *file);
GStrv  nautilus_column_get_default_column_order    (NautilusFile *file);
GStrv  nautilus_column_get_column_order            (NautilusFile *file);
