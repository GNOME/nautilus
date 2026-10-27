/*
 * SPDX-FileCopyrightText: 2011 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 */

#include "nautilus-progress-info-manager.h"

#include "nautilus-progress-info.h"

struct _NautilusProgressInfoManager
{
    GObject parent_instance;

    GList *progress_infos;
};

enum
{
    NEW_PROGRESS_INFO,
    LAST_SIGNAL
};

static NautilusProgressInfoManager *singleton = NULL;

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_FINAL_TYPE (NautilusProgressInfoManager, nautilus_progress_info_manager, G_TYPE_OBJECT);

static void
nautilus_progress_info_manager_finalize (GObject *obj)
{
    NautilusProgressInfoManager *self = NAUTILUS_PROGRESS_INFO_MANAGER (obj);

    if (self->progress_infos != NULL)
    {
        g_list_free_full (self->progress_infos, g_object_unref);
    }

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
}

NautilusProgressInfoManager *
nautilus_progress_info_manager_dup_singleton (void)
{
    return g_object_new (NAUTILUS_TYPE_PROGRESS_INFO_MANAGER, NULL);
}

static void
remove_progress_info (NautilusProgressInfo        *info,
                      NautilusProgressInfoManager *self)
{
    GList *link = g_list_find (self->progress_infos, info);

    if (link == NULL)
    {
        return;
    }

    self->progress_infos = g_list_delete_link (self->progress_infos, link);
    g_object_unref (info);
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

    /* The manager only keeps track of infos in progress, so that new windows
     * can pick them up. Each progress indicator holds on to its own infos. */
    g_signal_connect_object (info, "finished",
                             G_CALLBACK (remove_progress_info), self,
                             G_CONNECT_DEFAULT);
    g_signal_connect_object (info, "cancelled",
                             G_CALLBACK (remove_progress_info), self,
                             G_CONNECT_DEFAULT);

    g_signal_emit (self, signals[NEW_PROGRESS_INFO], 0, info);
}

GList *
nautilus_progress_info_manager_get_all_infos (NautilusProgressInfoManager *self)
{
    return self->progress_infos;
}
