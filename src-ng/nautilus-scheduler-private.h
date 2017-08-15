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

#ifndef NAUTILUS_SCHEDULER_PRIVATE_H_INCLUDED
#define NAUTILUS_SCHEDULER_PRIVATE_H_INCLUDED

#include "nautilus-scheduler.h"

typedef struct NautilusThreadWork NautilusThreadWork;

/**
 * nautilus_thread_work_run:
 * @work: an initialized #NautilusThreadWork
 */
void nautilus_thread_work_run (NautilusThreadWork *work);

/**
 * nautilus_thread_work_free:
 * @work: an initialized #NautilusThreadWork
 */
void                nautilus_thread_work_free (NautilusThreadWork *work);
/**
 * nautilus_thread_work_new:
 * @callback: the function to call
 * @data: data to pass to @callback
 *
 * Returns: (transfer full): a new #NautilusThreadWork instance
 */
NautilusThreadWork *nautilus_thread_work_new  (NautilusCallback    callback,
                                               gpointer            data);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (NautilusThreadWork, nautilus_thread_work_free)

#endif
