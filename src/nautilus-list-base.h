/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-files-view.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_LIST_BASE (nautilus_list_base_get_type())

G_DECLARE_DERIVABLE_TYPE (NautilusListBase, nautilus_list_base, NAUTILUS, LIST_BASE, NautilusFilesView)

struct _NautilusListBaseClass
{
        NautilusFilesViewClass parent_class;

        guint      (*get_icon_size)  (NautilusListBase *self);
        GtkWidget *(*get_view_ui)    (NautilusListBase *self);
        void       (*scroll_to_item) (NautilusListBase *self,
                                      guint                   position);
};

G_END_DECLS
