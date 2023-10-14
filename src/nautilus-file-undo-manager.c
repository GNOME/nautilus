/* nautilus-file-undo-manager.c - Manages the undo/redo stack
 *
 * Copyright (C) 2007-2011 Amos Brocco
 * Copyright (C) 2010, 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Amos Brocco <amos.brocco@gmail.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *
 */
#define G_LOG_DOMAIN "nautilus-undo"

#include <config.h>

#include "nautilus-file-undo-manager.h"

#include "nautilus-file-operations.h"
#include "nautilus-file.h"
#include "nautilus-trash-monitor.h"

#include <glib/gi18n.h>

enum
{
    SIGNAL_UNDO_CHANGED,
    NUM_SIGNALS,
};

static guint signals[NUM_SIGNALS] = { 0, };

struct _NautilusFileUndoManager
{
    GObject parent_instance;
    NautilusFileUndoInfo *info;
    NautilusFileUndoManagerState state;
    NautilusFileUndoManagerState last_state;

    guint is_operating : 1;

    gulong trash_signal_id;
};

G_DEFINE_TYPE (NautilusFileUndoManager, nautilus_file_undo_manager, G_TYPE_OBJECT)

static NautilusFileUndoManager *undo_singleton = NULL;

NautilusFileUndoManager *
nautilus_file_undo_manager_new (void)
{
    if (undo_singleton != NULL)
    {
        return g_object_ref (undo_singleton);
    }

    undo_singleton = g_object_new (NAUTILUS_TYPE_FILE_UNDO_MANAGER, NULL);
    g_object_add_weak_pointer (G_OBJECT (undo_singleton), (gpointer) & undo_singleton);

    return undo_singleton;
}

static void
file_undo_manager_clear (NautilusFileUndoManager *self)
{
    g_clear_object (&self->info);
    self->state = NAUTILUS_FILE_UNDO_MANAGER_STATE_NONE;
}

static void
trash_state_changed_cb (NautilusTrashMonitor *monitor,
                        gboolean              is_empty,
                        gpointer              user_data)
{
    NautilusFileUndoManager *self = user_data;

    /* A trash operation cannot be undone if the trash is empty */
    if (is_empty &&
        self->state == NAUTILUS_FILE_UNDO_MANAGER_STATE_UNDO &&
        NAUTILUS_IS_FILE_UNDO_INFO_TRASH (self->info))
    {
        file_undo_manager_clear (self);
        g_signal_emit (self, signals[SIGNAL_UNDO_CHANGED], 0);
    }
}

static void
nautilus_file_undo_manager_init (NautilusFileUndoManager *self)
{
    self->trash_signal_id = g_signal_connect (nautilus_trash_monitor_get (),
                                              "trash-state-changed",
                                              G_CALLBACK (trash_state_changed_cb), self);
}

static void
nautilus_file_undo_manager_finalize (GObject *object)
{
    NautilusFileUndoManager *self = NAUTILUS_FILE_UNDO_MANAGER (object);

    g_clear_signal_handler (&self->trash_signal_id, nautilus_trash_monitor_get ());

    file_undo_manager_clear (self);

    G_OBJECT_CLASS (nautilus_file_undo_manager_parent_class)->finalize (object);
}

static void
nautilus_file_undo_manager_class_init (NautilusFileUndoManagerClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = nautilus_file_undo_manager_finalize;

    signals[SIGNAL_UNDO_CHANGED] =
        g_signal_new ("undo-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
}

static void
undo_info_apply_ready (GObject      *source,
                       GAsyncResult *res,
                       gpointer      user_data)
{
    NautilusFileUndoManager *self = user_data;
    NautilusFileUndoInfo *info = NAUTILUS_FILE_UNDO_INFO (source);
    gboolean success, user_cancel;

    success = nautilus_file_undo_info_apply_finish (info, res, &user_cancel, NULL);

    self->is_operating = FALSE;

    /* just return in case we got another another operation set */
    if ((self->info != NULL) &&
        (self->info != info))
    {
        return;
    }

    if (success)
    {
        if (self->last_state == NAUTILUS_FILE_UNDO_MANAGER_STATE_UNDO)
        {
            self->state = NAUTILUS_FILE_UNDO_MANAGER_STATE_REDO;
        }
        else if (self->last_state == NAUTILUS_FILE_UNDO_MANAGER_STATE_REDO)
        {
            self->state = NAUTILUS_FILE_UNDO_MANAGER_STATE_UNDO;
        }

        self->info = g_object_ref (info);
    }
    else if (user_cancel)
    {
        self->state = self->last_state;
        self->info = g_object_ref (info);
    }
    else
    {
        file_undo_manager_clear (self);
    }

    g_signal_emit (self, signals[SIGNAL_UNDO_CHANGED], 0);
}

static void
do_undo_redo (NautilusFileUndoManager        *self,
              GtkWindow                      *parent_window,
              NautilusFileOperationsDBusData *dbus_data)
{
    gboolean undo = self->state == NAUTILUS_FILE_UNDO_MANAGER_STATE_UNDO;

    self->last_state = self->state;

    self->is_operating = TRUE;
    nautilus_file_undo_info_apply_async (self->info, undo, parent_window,
                                         dbus_data,
                                         undo_info_apply_ready, self);

    /* clear actions while undoing */
    file_undo_manager_clear (self);
    g_signal_emit (self, signals[SIGNAL_UNDO_CHANGED], 0);
}

void
nautilus_file_undo_manager_redo (GtkWindow                      *parent_window,
                                 NautilusFileOperationsDBusData *dbus_data)
{
    if (undo_singleton->state != NAUTILUS_FILE_UNDO_MANAGER_STATE_REDO)
    {
        g_warning ("Called redo, but state is %s!", undo_singleton->state == 0 ?
                   "none" : "undo");
        return;
    }

    do_undo_redo (undo_singleton, parent_window, dbus_data);
}

void
nautilus_file_undo_manager_undo (GtkWindow                      *parent_window,
                                 NautilusFileOperationsDBusData *dbus_data)
{
    if (undo_singleton->state != NAUTILUS_FILE_UNDO_MANAGER_STATE_UNDO)
    {
        g_warning ("Called undo, but state is %s!", undo_singleton->state == 0 ?
                   "none" : "redo");
        return;
    }

    do_undo_redo (undo_singleton, parent_window, dbus_data);
}

void
nautilus_file_undo_manager_set_action (NautilusFileUndoInfo *info)
{
    g_debug ("Setting undo information %p", info);

    file_undo_manager_clear (undo_singleton);

    if (info != NULL)
    {
        undo_singleton->info = g_object_ref (info);
        undo_singleton->state = NAUTILUS_FILE_UNDO_MANAGER_STATE_UNDO;
        undo_singleton->last_state = NAUTILUS_FILE_UNDO_MANAGER_STATE_NONE;
    }

    g_signal_emit (undo_singleton, signals[SIGNAL_UNDO_CHANGED], 0);
}

NautilusFileUndoInfo *
nautilus_file_undo_manager_get_action (void)
{
    return undo_singleton->info;
}

NautilusFileUndoManagerState
nautilus_file_undo_manager_get_state (void)
{
    return undo_singleton->state;
}


gboolean
nautilus_file_undo_manager_is_operating (void)
{
    return undo_singleton->is_operating;
}

NautilusFileUndoManager *
nautilus_file_undo_manager_get (void)
{
    return undo_singleton;
}
