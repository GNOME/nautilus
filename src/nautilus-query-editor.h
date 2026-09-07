/*
 * SPDX-FileCopyrightText: 2005 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#pragma once

#include <gtk/gtk.h>

#include "nautilus-types.h"

#define NAUTILUS_TYPE_QUERY_EDITOR nautilus_query_editor_get_type()

G_DECLARE_FINAL_TYPE (NautilusQueryEditor, nautilus_query_editor, NAUTILUS, QUERY_EDITOR, GtkWidget)

GtkWidget     *nautilus_query_editor_new          (void);

/**
 * nautilus_query_editor_set_query:
 *
 * @editor: A #NautilusQueryEditor instance.
 * @query: (nullable) (transfer full): The #NautilusQuery for the search.
 */
void           nautilus_query_editor_set_query    (NautilusQueryEditor *editor,
                                                   NautilusQuery       *query);
/**
 * nautilus_query_editor_set_location:
 *
 * @editor: A #NautilusQueryEditor instance.
 * @location: (nullable) (transfer full): The location in which the search will take place.
 */
void           nautilus_query_editor_set_location (NautilusQueryEditor *editor,
                                                   GFile               *location);
/**
 * nautilus_query_editor_select_all_text:
 *
 * @editor: A #NautilusQueryEditor instance.
 */
void           nautilus_query_editor_select_all_text (NautilusQueryEditor   *editor);

gboolean       nautilus_query_editor_handle_event    (NautilusQueryEditor   *self,
                                                      GtkEventControllerKey *controller,
                                                      guint                  keyval,
                                                      GdkModifierType        state);
