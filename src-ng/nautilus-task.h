/* Copyright (C) 2017 Ernestas Kulik <ernestask@gnome.org>
 *
 * This file is part of Nautilus.
 *
 * Nautilus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nautilus.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NAUTILUS_TASK_H
#define NAUTILUS_TASK_H

#include <gio/gio.h>
#include <glib-object.h>

#define NAUTILUS_TYPE_TASK (nautilus_task_get_type ())

G_DECLARE_DERIVABLE_TYPE (NautilusTask, nautilus_task,
                          NAUTILUS, TASK,
                          GObject)

typedef void (*NautilusTaskCallback) (NautilusTask *task,
                                      gpointer      user_data);

struct _NautilusTaskClass
{
    GObjectClass parent_class;

    void (*execute)                     (NautilusTask *task);
    void (*emit_signal_in_main_context) (NautilusTask  *instance,
                                         guint          signal_id,
                                         ...);
};

GCancellable *nautilus_task_get_cancellable (NautilusTask *task);

void nautilus_task_execute (NautilusTask *task);

#endif
