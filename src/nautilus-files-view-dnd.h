
/*
 * nautilus-view-dnd.h: DnD helpers for NautilusFilesView
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 * Copyright (C) 2000, 2001  Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ettore Perazzoli
 *          Darin Adler <darin@bentspoon.com>
 *          John Sullivan <sullivan@eazel.com>
 *          Pavel Cisler <pavel@eazel.com>
 */

#pragma once

#include "nautilus-files-view.h"

void nautilus_files_view_handle_uri_list_drop     (NautilusFilesView *view,
                                                   const char        *item_uris,
                                                   const char        *target_uri,
                                                   GdkDragAction      actions);
void nautilus_files_view_handle_text_drop         (NautilusFilesView *view,
                                                   const char        *text,
                                                   const char        *target_uri,
                                                   GdkDragAction      action);
void nautilus_files_view_handle_hover             (NautilusFilesView *view,
                                                   const char        *target_uri);

void nautilus_files_view_drop_proxy_received_uris (NautilusFilesView *view,
                                                   GList             *uris,
                                                   const char        *target_location,
                                                   GdkDragAction      action);
