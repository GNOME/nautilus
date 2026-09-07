
/*
 * SPDX-FileCopyrightText: 1999, 2000 Free Software Foundaton
 * SPDX-FileCopyrightText: 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
