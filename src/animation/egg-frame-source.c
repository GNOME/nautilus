/* egg-frame-source.c
 *
 * Copyright (C) 2010-2016 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "animation/egg-frame-source.h"

typedef struct
{
    GSource parent;
    guint fps;
    guint frame_count;
    gint64 start_time;
} EggFrameSource;

static gboolean
egg_frame_source_prepare (GSource *source,
                          gint    *timeout_)
{
    EggFrameSource *fsource = (EggFrameSource *) (gpointer) source;
    gint64 current_time;
    guint elapsed_time;
    guint new_frame_num;
    guint frame_time;

    current_time = g_source_get_time (source) / 1000;
    elapsed_time = current_time - fsource->start_time;
    new_frame_num = elapsed_time * fsource->fps / 1000;

    /* If time has gone backwards or the time since the last frame is
     * greater than the two frames worth then reset the time and do a
     * frame now */
    if (new_frame_num < fsource->frame_count ||
        new_frame_num - fsource->frame_count > 2)
    {
        /* Get the frame time rounded up to the nearest ms */
        frame_time = (1000 + fsource->fps - 1) / fsource->fps;

        /* Reset the start time */
        fsource->start_time = current_time;

        /* Move the start time as if one whole frame has elapsed */
        fsource->start_time -= frame_time;
        fsource->frame_count = 0;
        *timeout_ = 0;
        return TRUE;
    }
    else if (new_frame_num > fsource->frame_count)
    {
        *timeout_ = 0;
        return TRUE;
    }
    else
    {
        *timeout_ = (fsource->frame_count + 1) * 1000 / fsource->fps - elapsed_time;
        return FALSE;
    }
}

static gboolean
egg_frame_source_check (GSource *source)
{
    gint timeout_;
    return egg_frame_source_prepare (source, &timeout_);
}

static gboolean
egg_frame_source_dispatch (GSource     *source,
                           GSourceFunc  source_func,
                           gpointer     user_data)
{
    EggFrameSource *fsource = (EggFrameSource *) (gpointer) source;
    gboolean ret;

    if ((ret = source_func (user_data)))
    {
        fsource->frame_count++;
    }
    return ret;
}

static GSourceFuncs source_funcs =
{
    egg_frame_source_prepare,
    egg_frame_source_check,
    egg_frame_source_dispatch,
};

/**
 * egg_frame_source_add:
 * @frames_per_sec: (in): Target frames per second.
 * @callback: (in) (scope notified): A #GSourceFunc to execute.
 * @user_data: (in): User data for @callback.
 *
 * Creates a new frame source that will execute when the timeout interval
 * for the source has elapsed. The timing will try to synchronize based
 * on the end time of the animation.
 *
 * Returns: A source id that can be removed with g_source_remove().
 */
guint
egg_frame_source_add (guint       frames_per_sec,
                      GSourceFunc callback,
                      gpointer    user_data)
{
    EggFrameSource *fsource;
    GSource *source;
    guint ret;

    g_return_val_if_fail (frames_per_sec > 0, 0);
    g_return_val_if_fail (frames_per_sec <= 120, 0);

    source = g_source_new (&source_funcs, sizeof (EggFrameSource));
    fsource = (EggFrameSource *) (gpointer) source;
    fsource->fps = frames_per_sec;
    fsource->frame_count = 0;
    fsource->start_time = g_get_monotonic_time () / 1000;
    g_source_set_callback (source, callback, user_data, NULL);
    g_source_set_name (source, "EggFrameSource");

    ret = g_source_attach (source, NULL);
    g_source_unref (source);

    return ret;
}
