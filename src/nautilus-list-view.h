
/* fm-list-view.h - interface for list view of directory.

   Copyright (C) 2000 Eazel, Inc.
   Copyright (C) 2001 Anders Carlsson <andersca@gnu.org>
   
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

   Authors: John Sullivan <sullivan@eazel.com>
            Anders Carlsson <andersca@gnu.org>
*/

#pragma once

#include "nautilus-files-view.h"

#include "nautilus-dnd.h"
#include "nautilus-types.h"

#define NAUTILUS_TYPE_LIST_VIEW (nautilus_list_view_get_type ())
G_DECLARE_FINAL_TYPE (NautilusListView, nautilus_list_view, NAUTILUS, LIST_VIEW, NautilusFilesView)

NautilusDragInfo  *nautilus_list_view_dnd_get_drag_source_data (NautilusListView   *view);

NautilusFilesView *nautilus_list_view_new                      (NautilusWindowSlot *slot);
