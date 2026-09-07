
/*
 * nautilus-view-dnd.h: DnD helpers for NautilusFilesView
 *
 * SPDX-FileCopyrightText: 1999, 2000 Free Software Foundaton
 * SPDX-FileCopyrightText: 2000, 2001 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Ettore Perazzoli
 *          Darin Adler <darin@bentspoon.com>
 *          John Sullivan <sullivan@eazel.com>
 *          Pavel Cisler <pavel@eazel.com>
 */

#pragma once

#include "nautilus-types.h"

#include <gtk/gtk.h>

void nautilus_files_view_handle_text_drop         (NautilusFilesView *view,
                                                   const char        *text,
                                                   const char        *target_uri,
                                                   GdkDragAction      action);

void nautilus_files_view_drop_proxy_received_uris (NautilusFilesView *view,
                                                   const GList       *uris,
                                                   const char        *target_location,
                                                   GdkDragAction      action);
