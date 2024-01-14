/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

#include "nautilus-view-info.h"
#include "nautilus-view-item.h"
#include "nautilus-view-model.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_LIST_BASE (nautilus_list_base_get_type())

G_DECLARE_DERIVABLE_TYPE (NautilusListBase, nautilus_list_base, NAUTILUS, LIST_BASE, AdwBin)

struct _NautilusListBaseClass
{
        AdwBinClass parent_class;

        /* Subclasses must provide implementation. */
        NautilusViewInfo (*get_view_info)     (NautilusListBase *self);
        guint      (*get_icon_size)  (NautilusListBase *self);
        GVariant  *(*get_sort_state)          (NautilusListBase *self);
        GtkWidget *(*get_view_ui)    (NautilusListBase *self);
        int        (*get_zoom_level)          (NautilusListBase *self);
        void       (*scroll_to)      (NautilusListBase   *self,
                                      guint               position,
                                      GtkListScrollFlags  flags,
                                      GtkScrollInfo      *scroll);
        void       (*set_sort_state)          (NautilusListBase *self,
                                               GVariant         *sort_state);
        void       (*set_zoom_level)          (NautilusListBase *self,
                                               int               new_zoom_level);

        /* Subclasses may override base implementation. */
        NautilusViewItem *(*get_backing_item) (NautilusListBase *self);
        void       (*preview_selection_event) (NautilusListBase *self,
                                               GtkDirectionType  direction);

        /* Subclass override must chain-up to base implementation. */
        void       (*setup_directory) (NautilusListBase  *self,
                                       NautilusDirectory *directory);
};

NautilusViewItem *nautilus_list_base_get_backing_item (NautilusListBase *self);
GtkWidget *nautilus_list_base_get_selected_item_ui (NautilusListBase  *self);
GVariant *nautilus_list_base_get_sort_state        (NautilusListBase  *self);
GtkAdjustment *nautilus_list_base_get_vadjustment  (NautilusListBase  *self);
NautilusViewInfo nautilus_list_base_get_view_info  (NautilusListBase  *self) G_GNUC_PURE;
int     nautilus_list_base_get_zoom_level          (NautilusListBase  *self);
void nautilus_list_base_preview_selection_event (NautilusListBase *self,
                                                 GtkDirectionType  direction);
void nautilus_list_base_set_cursor (NautilusListBase *self,
                                    guint             position,
                                    gboolean          select,
                                    gboolean          scroll_to);
void    nautilus_list_base_set_model               (NautilusListBase  *self,
                                                    NautilusViewModel *model);
void    nautilus_list_base_set_sort_state          (NautilusListBase  *self,
                                                    GVariant          *sort_state);
void    nautilus_list_base_set_zoom_level          (NautilusListBase  *self,
                                                    int                new_zoom_level);
void    nautilus_list_base_setup_directory         (NautilusListBase  *self,
                                                    NautilusDirectory *directory);

G_END_DECLS
