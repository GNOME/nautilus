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
 * along with Nautilus.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef NAUTILUS_SCHEDULER_H_INCLUDED
#define NAUTILUS_SCHEDULER_H_INCLUDED

#include <glib-object.h>

#define NAUTILUS_CALLBACK(x)    ((NautilusCallback) x)
#define NAUTILUS_TYPE_SCHEDULER (nautilus_scheduler_get_type ())

G_DECLARE_DERIVABLE_TYPE (NautilusScheduler, nautilus_scheduler,
                          NAUTILUS, SCHEDULER, GObject)

/**
 * NautilusCallback:
 * @data: (nullable): user data passed to #nautilus_scheduler_queue
 */
typedef void (*NautilusCallback) (gpointer data);

struct _NautilusSchedulerClass
{
    GObjectClass parent_class;

    void (*queue) (NautilusScheduler *scheduler,
                   NautilusCallback   func,
                   gpointer           func_data);
};

/**
 * nautilus_scheduler_queue:
 * @scheduler: an initialized #NautilusScheduler
 * @func: the #NautilusCallback to call
 * @func_data: (nullable): additional data to pass to @func
 */
void nautilus_scheduler_queue (NautilusScheduler *scheduler,
                               NautilusCallback   func,
                               gpointer           func_data);

/**
 * nautilus_scheduler_get_default:
 *
 * Returns: (transfer full): the default #NautilusScheduler instance
 */
NautilusScheduler *nautilus_scheduler_get_default (void);

#endif
