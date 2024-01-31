/*
 *  nautilus-progress-info.h: file operation progress info.
 *
 *  Copyright (C) 2007 Red Hat, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *  Author: Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>
#include <math.h>
#include <glib/gi18n.h>
#include "nautilus-progress-info.h"
#include "nautilus-progress-info-manager.h"
#include "nautilus-icon-info.h"

enum
{
    CHANGED,
    PROGRESS_CHANGED,
    STARTED,
    FINISHED,
    CANCELLED,
    LAST_SIGNAL
};

#define SIGNAL_DELAY_MSEC 100

static guint signals[LAST_SIGNAL] = { 0 };

enum
{
    PROP_0,
    PROP_ICON_NAME,
    PROP_PROGRESS,
    PROP_STATUS,
    N_PROPS
};

GParamSpec *properties[N_PROPS] = { NULL, };

struct _NautilusProgressInfo
{
    GObject parent_instance;

    GCancellable *cancellable;
    guint cancellable_id;

    GTimer *progress_timer;

    char *status;
    char *details;
    double progress;
    gdouble remaining_time;
    gdouble elapsed_time;
    gboolean activity_mode;
    gboolean started;
    gboolean finished;
    gboolean paused;

    GSource *idle_source;
    gboolean source_is_now;

    gboolean start_at_idle;
    gboolean finish_at_idle;
    gboolean cancel_at_idle;
    gboolean changed_at_idle;
    gboolean progress_at_idle;

    GFile *destination;
};

G_LOCK_DEFINE_STATIC (progress_info);

G_DEFINE_TYPE (NautilusProgressInfo, nautilus_progress_info, G_TYPE_OBJECT)

static void set_details (NautilusProgressInfo *info,
                         const char           *details);
static void set_status (NautilusProgressInfo *info,
                        const char           *status);
static const char * get_icon_name (NautilusProgressInfo *info);

static void
nautilus_progress_info_finalize (GObject *object)
{
    NautilusProgressInfo *info;

    info = NAUTILUS_PROGRESS_INFO (object);

    g_free (info->status);
    g_free (info->details);
    g_clear_pointer (&info->progress_timer, g_timer_destroy);
    g_cancellable_disconnect (info->cancellable, info->cancellable_id);
    g_object_unref (info->cancellable);
    g_clear_object (&info->destination);

    if (G_OBJECT_CLASS (nautilus_progress_info_parent_class)->finalize)
    {
        (*G_OBJECT_CLASS (nautilus_progress_info_parent_class)->finalize)(object);
    }
}

static void
nautilus_progress_info_dispose (GObject *object)
{
    NautilusProgressInfo *info;

    info = NAUTILUS_PROGRESS_INFO (object);

    G_LOCK (progress_info);

    /* Destroy source in dispose, because the callback
     *  could come here before the destroy, which should
     *  ressurect the object for a while */
    if (info->idle_source)
    {
        g_source_destroy (info->idle_source);
        g_source_unref (info->idle_source);
        info->idle_source = NULL;
    }
    G_UNLOCK (progress_info);
}

static void
nautilus_progress_info_get_property (GObject    *object,
                                     guint       property_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
    NautilusProgressInfo *self = NAUTILUS_PROGRESS_INFO (object);

    switch (property_id)
    {
        case (PROP_ICON_NAME):
        {
            g_value_set_string (value, get_icon_name (self));
        }
        break;

        case (PROP_PROGRESS):
        {
            G_LOCK (progress_info);
            /* This is a little different than nautilus_progress_info_get_progress ()
             * in that we don't want to ever return -1, which would be the case when
             * activity mode is true
             */
            g_value_set_double (value, self->progress);
            G_UNLOCK (progress_info);
        }
        break;

        case (PROP_STATUS):
        {
            g_value_take_string (value, nautilus_progress_info_get_status (self));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

static void
nautilus_progress_info_class_init (NautilusProgressInfoClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = nautilus_progress_info_finalize;
    gobject_class->dispose = nautilus_progress_info_dispose;
    gobject_class->get_property = nautilus_progress_info_get_property;

    signals[CHANGED] =
        g_signal_new ("changed",
                      NAUTILUS_TYPE_PROGRESS_INFO,
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[PROGRESS_CHANGED] =
        g_signal_new ("progress-changed",
                      NAUTILUS_TYPE_PROGRESS_INFO,
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[STARTED] =
        g_signal_new ("started",
                      NAUTILUS_TYPE_PROGRESS_INFO,
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[FINISHED] =
        g_signal_new ("finished",
                      NAUTILUS_TYPE_PROGRESS_INFO,
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[CANCELLED] =
        g_signal_new ("cancelled",
                      NAUTILUS_TYPE_PROGRESS_INFO,
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    properties[PROP_ICON_NAME] = g_param_spec_string ("icon-name",
                                                      NULL, NULL,
                                                      NULL,
                                                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_PROGRESS] = g_param_spec_double ("progress", NULL, NULL,
                                                     0, G_MAXDOUBLE, 0,
                                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_STATUS] = g_param_spec_string ("status",
                                                   NULL, NULL,
                                                   "",
                                                   G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static gboolean
idle_callback (gpointer data)
{
    NautilusProgressInfo *info = data;
    gboolean start_at_idle;
    gboolean finish_at_idle;
    gboolean changed_at_idle;
    gboolean progress_at_idle;
    gboolean cancelled_at_idle;
    GSource *source;

    G_LOCK (progress_info);

    source = g_main_current_source ();

    /* Protect agains races where the source has
     *  been destroyed on another thread while it
     *  was being dispatched.
     *  Similar to what gdk_threads_add_idle does.
     */
    if (g_source_is_destroyed (source))
    {
        G_UNLOCK (progress_info);
        return FALSE;
    }

    /* We hadn't destroyed the source, so take a ref.
     * This might ressurect the object from dispose, but
     * that should be ok.
     */
    g_object_ref (info);

    g_assert (source == info->idle_source);

    g_source_unref (source);
    info->idle_source = NULL;

    start_at_idle = info->start_at_idle;
    finish_at_idle = info->finish_at_idle;
    changed_at_idle = info->changed_at_idle;
    progress_at_idle = info->progress_at_idle;
    cancelled_at_idle = info->cancel_at_idle;

    info->start_at_idle = FALSE;
    info->finish_at_idle = FALSE;
    info->changed_at_idle = FALSE;
    info->progress_at_idle = FALSE;
    info->cancel_at_idle = FALSE;

    G_UNLOCK (progress_info);

    if (start_at_idle)
    {
        g_signal_emit (info,
                       signals[STARTED],
                       0);
    }

    if (changed_at_idle)
    {
        g_object_notify (G_OBJECT (info), "status");
        g_signal_emit (info,
                       signals[CHANGED],
                       0);
    }

    if (progress_at_idle)
    {
        g_signal_emit (info,
                       signals[PROGRESS_CHANGED],
                       0);
        g_object_notify (G_OBJECT (info), "progress");
    }

    if (finish_at_idle)
    {
        g_object_notify (G_OBJECT (info), "icon-name");
        g_signal_emit (info,
                       signals[FINISHED],
                       0);
    }

    if (cancelled_at_idle)
    {
        g_object_notify (G_OBJECT (info), "icon-name");
        g_signal_emit (info,
                       signals[CANCELLED],
                       0);
    }

    g_object_unref (info);

    return FALSE;
}


/* Called with lock held */
static void
queue_idle (NautilusProgressInfo *info,
            gboolean              now)
{
    if (info->idle_source == NULL ||
        (now && !info->source_is_now))
    {
        if (info->idle_source)
        {
            g_source_destroy (info->idle_source);
            g_source_unref (info->idle_source);
            info->idle_source = NULL;
        }

        info->source_is_now = now;
        if (now)
        {
            info->idle_source = g_idle_source_new ();
        }
        else
        {
            info->idle_source = g_timeout_source_new (SIGNAL_DELAY_MSEC);
        }
        g_source_set_callback (info->idle_source, idle_callback, info, NULL);
        g_source_attach (info->idle_source, NULL);
    }
}

static void
on_canceled (GCancellable         *cancellable,
             NautilusProgressInfo *info)
{
    G_LOCK (progress_info);
    set_details (info, _("Canceled"));
    info->cancel_at_idle = TRUE;
    g_timer_stop (info->progress_timer);
    queue_idle (info, TRUE);
    G_UNLOCK (progress_info);
}

static void
nautilus_progress_info_init (NautilusProgressInfo *info)
{
    NautilusProgressInfoManager *manager;

    info->cancellable = g_cancellable_new ();
    info->cancellable_id = g_cancellable_connect (info->cancellable,
                                                  G_CALLBACK (on_canceled),
                                                  info,
                                                  NULL);

    manager = nautilus_progress_info_manager_dup_singleton ();
    nautilus_progress_info_manager_add_new_info (manager, info);
    g_object_unref (manager);
    info->progress_timer = g_timer_new ();
}

NautilusProgressInfo *
nautilus_progress_info_new (void)
{
    NautilusProgressInfo *info;

    info = g_object_new (NAUTILUS_TYPE_PROGRESS_INFO, NULL);

    return info;
}

char *
nautilus_progress_info_get_status (NautilusProgressInfo *info)
{
    char *res;

    G_LOCK (progress_info);

    if (info->status)
    {
        res = g_strdup (info->status);
    }
    else
    {
        res = g_strdup (_("Preparing"));
    }

    G_UNLOCK (progress_info);

    return res;
}

char *
nautilus_progress_info_get_details (NautilusProgressInfo *info)
{
    char *res;

    G_LOCK (progress_info);

    if (info->details)
    {
        res = g_strdup (info->details);
    }
    else
    {
        res = g_strdup (_("Preparing"));
    }

    G_UNLOCK (progress_info);

    return res;
}

double
nautilus_progress_info_get_progress (NautilusProgressInfo *info)
{
    double res;

    G_LOCK (progress_info);

    if (info->activity_mode)
    {
        res = -1.0;
    }
    else
    {
        res = info->progress;
    }

    G_UNLOCK (progress_info);

    return res;
}

void
nautilus_progress_info_cancel (NautilusProgressInfo *info)
{
    GCancellable *cancellable;

    cancellable = nautilus_progress_info_get_cancellable (info);
    g_cancellable_cancel (cancellable);
    g_object_unref (cancellable);
}

GCancellable *
nautilus_progress_info_get_cancellable (NautilusProgressInfo *info)
{
    GCancellable *c;

    G_LOCK (progress_info);

    c = g_object_ref (info->cancellable);

    G_UNLOCK (progress_info);

    return c;
}

gboolean
nautilus_progress_info_get_is_cancelled (NautilusProgressInfo *info)
{
    gboolean cancelled;

    G_LOCK (progress_info);
    cancelled = g_cancellable_is_cancelled (info->cancellable);
    G_UNLOCK (progress_info);

    return cancelled;
}

gboolean
nautilus_progress_info_get_is_started (NautilusProgressInfo *info)
{
    gboolean res;

    G_LOCK (progress_info);

    res = info->started;

    G_UNLOCK (progress_info);

    return res;
}

gboolean
nautilus_progress_info_get_is_finished (NautilusProgressInfo *info)
{
    gboolean res;

    G_LOCK (progress_info);

    res = info->finished;

    G_UNLOCK (progress_info);

    return res;
}

static const char *
get_icon_name (NautilusProgressInfo *info)
{
    if (nautilus_progress_info_get_is_cancelled (info))
    {
        return "file-operation-cancelled-symbolic";
    }
    else if (nautilus_progress_info_get_is_finished (info))
    {
        return "file-operation-finished-symbolic";
    }

    return NULL;
}

gboolean
nautilus_progress_info_get_is_paused (NautilusProgressInfo *info)
{
    gboolean res;

    G_LOCK (progress_info);

    res = info->paused;

    G_UNLOCK (progress_info);

    return res;
}

void
nautilus_progress_info_pause (NautilusProgressInfo *info)
{
    G_LOCK (progress_info);

    if (!info->paused)
    {
        info->paused = TRUE;
        g_timer_stop (info->progress_timer);
    }

    G_UNLOCK (progress_info);
}

void
nautilus_progress_info_resume (NautilusProgressInfo *info)
{
    G_LOCK (progress_info);

    if (info->paused)
    {
        info->paused = FALSE;
        g_timer_continue (info->progress_timer);
    }

    G_UNLOCK (progress_info);
}

void
nautilus_progress_info_start (NautilusProgressInfo *info)
{
    G_LOCK (progress_info);

    if (!info->started)
    {
        info->started = TRUE;
        g_timer_start (info->progress_timer);

        info->start_at_idle = TRUE;
        queue_idle (info, TRUE);
    }

    G_UNLOCK (progress_info);
}

void
nautilus_progress_info_finish (NautilusProgressInfo *info)
{
    G_LOCK (progress_info);

    if (!info->finished)
    {
        info->finished = TRUE;
        g_timer_stop (info->progress_timer);

        info->finish_at_idle = TRUE;
        queue_idle (info, TRUE);
    }

    G_UNLOCK (progress_info);
}

static void
set_status (NautilusProgressInfo *info,
            const char           *status)
{
    g_free (info->status);
    info->status = g_strdup (status);

    info->changed_at_idle = TRUE;
    queue_idle (info, FALSE);
}

void
nautilus_progress_info_take_status (NautilusProgressInfo *info,
                                    char                 *status)
{
    G_LOCK (progress_info);

    if (g_strcmp0 (info->status, status) != 0 &&
        !g_cancellable_is_cancelled (info->cancellable))
    {
        set_status (info, status);
    }

    G_UNLOCK (progress_info);

    g_free (status);
}

void
nautilus_progress_info_set_status (NautilusProgressInfo *info,
                                   const char           *status)
{
    G_LOCK (progress_info);

    if (g_strcmp0 (info->status, status) != 0 &&
        !g_cancellable_is_cancelled (info->cancellable))
    {
        set_status (info, status);
    }

    G_UNLOCK (progress_info);
}

static void
set_details (NautilusProgressInfo *info,
             const char           *details)
{
    g_free (info->details);
    info->details = g_strdup (details);

    info->changed_at_idle = TRUE;
    queue_idle (info, FALSE);
}

void
nautilus_progress_info_take_details (NautilusProgressInfo *info,
                                     char                 *details)
{
    G_LOCK (progress_info);

    if (g_strcmp0 (info->details, details) != 0 &&
        !g_cancellable_is_cancelled (info->cancellable))
    {
        set_details (info, details);
    }

    G_UNLOCK (progress_info);

    g_free (details);
}

void
nautilus_progress_info_set_details (NautilusProgressInfo *info,
                                    const char           *details)
{
    G_LOCK (progress_info);

    if (g_strcmp0 (info->details, details) != 0 &&
        !g_cancellable_is_cancelled (info->cancellable))
    {
        set_details (info, details);
    }

    G_UNLOCK (progress_info);
}

void
nautilus_progress_info_pulse_progress (NautilusProgressInfo *info)
{
    G_LOCK (progress_info);

    info->activity_mode = TRUE;
    info->progress = 0.0;
    info->progress_at_idle = TRUE;
    queue_idle (info, FALSE);

    G_UNLOCK (progress_info);
}

void
nautilus_progress_info_set_progress (NautilusProgressInfo *info,
                                     double                current,
                                     double                total)
{
    double current_percent;

    if (total <= 0)
    {
        current_percent = 1.0;
    }
    else
    {
        current_percent = current / total;

        if (current_percent < 0)
        {
            current_percent = 0;
        }

        if (current_percent > 1.0)
        {
            current_percent = 1.0;
        }
    }

    G_LOCK (progress_info);

    if ((info->activity_mode ||     /* emit on switch from activity mode */
         fabs (current_percent - info->progress) > 0.005) &&    /* Emit on change of 0.5 percent */
        !g_cancellable_is_cancelled (info->cancellable))
    {
        info->activity_mode = FALSE;
        info->progress = current_percent;
        info->progress_at_idle = TRUE;
        queue_idle (info, FALSE);
    }

    G_UNLOCK (progress_info);
}

void
nautilus_progress_info_set_remaining_time (NautilusProgressInfo *info,
                                           gdouble               time)
{
    G_LOCK (progress_info);
    info->remaining_time = time;
    G_UNLOCK (progress_info);
}

gdouble
nautilus_progress_info_get_remaining_time (NautilusProgressInfo *info)
{
    gint remaining_time;

    G_LOCK (progress_info);
    remaining_time = info->remaining_time;
    G_UNLOCK (progress_info);

    return remaining_time;
}

void
nautilus_progress_info_set_elapsed_time (NautilusProgressInfo *info,
                                         gdouble               time)
{
    G_LOCK (progress_info);
    info->elapsed_time = time;
    G_UNLOCK (progress_info);
}

gdouble
nautilus_progress_info_get_elapsed_time (NautilusProgressInfo *info)
{
    gint elapsed_time;

    G_LOCK (progress_info);
    elapsed_time = info->elapsed_time;
    G_UNLOCK (progress_info);

    return elapsed_time;
}

gdouble
nautilus_progress_info_get_total_elapsed_time (NautilusProgressInfo *info)
{
    gdouble elapsed_time = 0;

    G_LOCK (progress_info);
    if (info->progress_timer != NULL)
    {
        elapsed_time = g_timer_elapsed (info->progress_timer, NULL);
    }
    G_UNLOCK (progress_info);

    return elapsed_time;
}

void
nautilus_progress_info_set_destination (NautilusProgressInfo *info,
                                        GFile                *file)
{
    G_LOCK (progress_info);
    g_clear_object (&info->destination);
    info->destination = g_object_ref (file);
    G_UNLOCK (progress_info);
}

GFile *
nautilus_progress_info_get_destination (NautilusProgressInfo *info)
{
    GFile *destination = NULL;

    G_LOCK (progress_info);
    if (info->destination)
    {
        destination = g_object_ref (info->destination);
    }
    G_UNLOCK (progress_info);

    return destination;
}
