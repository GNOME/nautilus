/*
 * Nautilus
 *
 * Copyright (C) 2011 Red Hat, Inc.
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
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 */

#include <config.h>

#include "nautilus-progress-info-manager.h"

struct _NautilusProgressInfoManager
{
    GObject parent_instance;

    GList *progress_infos;
    GList *current_viewers;
};

enum
{
    NEW_PROGRESS_INFO,
    HAS_VIEWERS_CHANGED,
    LAST_SIGNAL
};

static NautilusProgressInfoManager *singleton = NULL;

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (NautilusProgressInfoManager, nautilus_progress_info_manager,
               G_TYPE_OBJECT);

static void remove_viewer (NautilusProgressInfoManager *self,
                           GObject                     *viewer);

static void
nautilus_progress_info_manager_finalize (GObject *obj)
{
    GList *l;
    NautilusProgressInfoManager *self = NAUTILUS_PROGRESS_INFO_MANAGER (obj);

    if (self->progress_infos != NULL)
    {
        g_list_free_full (self->progress_infos, g_object_unref);
    }

    for (l = self->current_viewers; l != NULL; l = l->next)
    {
        g_object_weak_unref (l->data, (GWeakNotify) remove_viewer, self);
    }
    g_list_free (self->current_viewers);

    G_OBJECT_CLASS (nautilus_progress_info_manager_parent_class)->finalize (obj);
}

static GObject *
nautilus_progress_info_manager_constructor (GType                  type,
                                            guint                  n_props,
                                            GObjectConstructParam *props)
{
    GObject *retval;

    if (singleton != NULL)
    {
        return G_OBJECT (g_object_ref (singleton));
    }

    retval = G_OBJECT_CLASS (nautilus_progress_info_manager_parent_class)->constructor
                 (type, n_props, props);

    singleton = NAUTILUS_PROGRESS_INFO_MANAGER (retval);
    g_object_add_weak_pointer (retval, (gpointer) & singleton);

    return retval;
}

static void
nautilus_progress_info_manager_init (NautilusProgressInfoManager *self)
{
}

static void
nautilus_progress_info_manager_class_init (NautilusProgressInfoManagerClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);
    oclass->constructor = nautilus_progress_info_manager_constructor;
    oclass->finalize = nautilus_progress_info_manager_finalize;

    signals[NEW_PROGRESS_INFO] =
        g_signal_new ("new-progress-info",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE,
                      1,
                      NAUTILUS_TYPE_PROGRESS_INFO);

    signals[HAS_VIEWERS_CHANGED] =
        g_signal_new ("has-viewers-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);
}

NautilusProgressInfoManager *
nautilus_progress_info_manager_dup_singleton (void)
{
    return g_object_new (NAUTILUS_TYPE_PROGRESS_INFO_MANAGER, NULL);
}

void
nautilus_progress_info_manager_add_new_info (NautilusProgressInfoManager *self,
                                             NautilusProgressInfo        *info)
{
    if (g_list_find (self->progress_infos, info) != NULL)
    {
        g_warning ("Adding two times the same progress info object to the manager");
        return;
    }

    self->progress_infos =
        g_list_prepend (self->progress_infos, g_object_ref (info));

    g_signal_emit (self, signals[NEW_PROGRESS_INFO], 0, info);
}

GList *
nautilus_progress_info_manager_get_all_infos (NautilusProgressInfoManager *self)
{
    return self->progress_infos;
}

static void
remove_viewer (NautilusProgressInfoManager *self,
               GObject                     *viewer)
{
    self->current_viewers = g_list_remove (self->current_viewers, viewer);

    if (self->current_viewers == NULL)
    {
        g_signal_emit (self, signals[HAS_VIEWERS_CHANGED], 0);
    }
}

void
nautilus_progress_manager_add_viewer (NautilusProgressInfoManager *self,
                                      GObject                     *viewer)
{
    GList *viewers;

    viewers = self->current_viewers;
    if (g_list_find (viewers, viewer) == NULL)
    {
        g_object_weak_ref (viewer, (GWeakNotify) remove_viewer, self);
        viewers = g_list_append (viewers, viewer);
        self->current_viewers = viewers;

        if (g_list_length (viewers) == 1)
        {
            g_signal_emit (self, signals[HAS_VIEWERS_CHANGED], 0);
        }
    }
}

void
nautilus_progress_manager_remove_viewer (NautilusProgressInfoManager *self,
                                         GObject                     *viewer)
{
    if (g_list_find (self->current_viewers, viewer) != NULL)
    {
        g_object_weak_unref (viewer, (GWeakNotify) remove_viewer, self);
        remove_viewer (self, viewer);
    }
}

gboolean
nautilus_progress_manager_has_viewers (NautilusProgressInfoManager *self)
{
    return self->current_viewers != NULL;
}
