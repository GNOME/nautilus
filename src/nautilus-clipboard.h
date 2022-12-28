
/* fm-directory-view.h
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 * Copyright (C) 2000  Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Rebecca Schulman <rebecka@eazel.com>
 */

#pragma once

#include <gtk/gtk.h>

typedef struct _NautilusClipboard NautilusClipboard;
#define NAUTILUS_TYPE_CLIPBOARD (nautilus_clipboard_get_type())
GType              nautilus_clipboard_get_type     (void);

void nautilus_clipboard_clear_if_colliding_uris    (GtkWidget          *widget,
                                                    const GList        *item_uris);
GList             *nautilus_clipboard_peek_files   (NautilusClipboard *clip);
GList             *nautilus_clipboard_get_uri_list (NautilusClipboard *clip);
gboolean           nautilus_clipboard_is_cut       (NautilusClipboard *clip);

NautilusClipboard *nautilus_clipboard_copy         (NautilusClipboard *clip);
void               nautilus_clipboard_free         (NautilusClipboard *clip);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (NautilusClipboard, nautilus_clipboard_free)

void nautilus_clipboard_prepare_for_files (GdkClipboard *clipboard,
                                           GList        *files,
                                           gboolean      cut);

void               nautilus_clipboard_register     (void);
