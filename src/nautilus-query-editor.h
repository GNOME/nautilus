/*
 * Copyright (C) 2005 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#pragma once

#include <gtk/gtk.h>

#include "nautilus-query.h"

#define NAUTILUS_TYPE_QUERY_EDITOR nautilus_query_editor_get_type()

G_DECLARE_DERIVABLE_TYPE (NautilusQueryEditor, nautilus_query_editor, NAUTILUS, QUERY_EDITOR, GtkBox)

struct _NautilusQueryEditorClass
{
    GtkBoxClass parent_class;

    void (* changed)   (NautilusQueryEditor  *editor,
                        NautilusQuery        *query,
                        gboolean              reload);
    void (* cancel)    (NautilusQueryEditor *editor);
    void (* activated) (NautilusQueryEditor *editor);
};

GtkWidget     *nautilus_query_editor_new          (void);

/**
 * nautilus_query_editor_get_query:
 *
 * @editor: A #NautilusQueryEditor instance.
 *
 * Returns: (nullable) (transfer full): The #NautilusQuery for the editor.
 */
NautilusQuery *nautilus_query_editor_get_query    (NautilusQueryEditor *editor);
/**
 * nautilus_query_editor_set_query:
 *
 * @editor: A #NautilusQueryEditor instance.
 * @query: (nullable) (transfer full): The #NautilusQuery for the search.
 */
void           nautilus_query_editor_set_query    (NautilusQueryEditor *editor,
                                                   NautilusQuery       *query);
/**
 * nautilus_query_editor_get_location:
 *
 * @editor: A #NautilusQueryEditor instance.
 *
 * Returns: (nullable) (transfer full): The location of the current search.
 */
GFile         *nautilus_query_editor_get_location (NautilusQueryEditor *editor);
/**
 * nautilus_query_editor_set_location:
 *
 * @editor: A #NautilusQueryEditor instance.
 * @location: (nullable) (transfer full): The location in which the search will take place.
 */
void           nautilus_query_editor_set_location (NautilusQueryEditor *editor,
                                                   GFile               *location);
/**
 * nautilus_query_editor_set_text:
 *
 * @editor: A #NautilusQueryEditor instance.
 * @text: (not nullable) (transfer none): The search text.
 */
void           nautilus_query_editor_set_text     (NautilusQueryEditor *editor,
                                                   const gchar         *text);


void           nautilus_query_editor_show_popover (NautilusQueryEditor *editor);

gboolean
nautilus_query_editor_handle_event (NautilusQueryEditor *self,
                                    GdkEvent            *event);
