/* Copyright (C) 2018 Ernestas Kulik <ernestask@gnome.org>
 *
 * This file is part of Nautilus.
 *
 * Nautilus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#pragma once

#include "nautilus-types.h"

#include <gdk/gdk.h>

#define NAUTILUS_TYPE_CONTENT_PROVIDER     nautilus_content_provider_get_type     ()
#define NAUTILUS_TYPE_CUT_CONTENT_PROVIDER nautilus_cut_content_provider_get_type ()
#define NAUTILUS_TYPE_FILE_LIST            nautilus_file_list_get_type            ()

struct _NautilusContentProviderClass
{
    GdkContentProviderClass parent_class;
};

G_DECLARE_DERIVABLE_TYPE (NautilusContentProvider, nautilus_content_provider,
                          NAUTILUS, CONTENT_PROVIDER,
                          GdkContentProvider)
G_DECLARE_FINAL_TYPE (NautilusCutContentProvider, nautilus_cut_content_provider,
                      NAUTILUS, CUT_CONTENT_PROVIDER,
                      NautilusContentProvider)

GList              *nautilus_content_provider_get_files         (NautilusContentProvider *provider);

GdkContentProvider *nautilus_content_provider_new_for_selection (NautilusView            *view,
                                                                 gboolean                 cut);
