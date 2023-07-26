/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-files-view.h"
#include "nautilus-view-item.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_LIST_BASE (nautilus_list_base_get_type())

G_DECLARE_DERIVABLE_TYPE (NautilusListBase, nautilus_list_base, NAUTILUS, LIST_BASE, NautilusFilesView)

struct _NautilusListBaseClass
{
        NautilusFilesViewClass parent_class;

        guint      (*get_icon_size)  (NautilusListBase *self);
        GtkWidget *(*get_view_ui)    (NautilusListBase *self);
        void       (*preview_selection_event) (NautilusListBase *self,
                                               GtkDirectionType  direction);
        void       (*scroll_to)      (NautilusListBase   *self,
                                      guint               position,
                                      GtkListScrollFlags  flags,
                                      GtkScrollInfo      *scroll);
};

void nautilus_list_base_preview_selection_event (NautilusListBase *self,
                                                 GtkDirectionType  direction);
void nautilus_list_base_set_cursor (NautilusListBase *self,
                                    guint             position,
                                    gboolean          select,
                                    gboolean          scroll_to);

G_END_DECLS
