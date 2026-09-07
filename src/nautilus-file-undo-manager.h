
/*
 * SPDX-FileCopyrightText: 2007-2011 Amos Brocco
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Amos Brocco <amos.brocco@gmail.com>
 */

#pragma once

#include "nautilus-types.h"

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#define NAUTILUS_TYPE_FILE_UNDO_MANAGER\
	(nautilus_file_undo_manager_get_type())

G_DECLARE_FINAL_TYPE (NautilusFileUndoManager, nautilus_file_undo_manager, NAUTILUS, FILE_UNDO_MANAGER, GObject)

typedef enum {
	NAUTILUS_FILE_UNDO_MANAGER_STATE_NONE,
	NAUTILUS_FILE_UNDO_MANAGER_STATE_UNDO,
	NAUTILUS_FILE_UNDO_MANAGER_STATE_REDO
} NautilusFileUndoManagerState;

NautilusFileUndoManager *nautilus_file_undo_manager_new (void);
NautilusFileUndoManager * nautilus_file_undo_manager_get (void);

void nautilus_file_undo_manager_set_action (NautilusFileUndoInfo *info);
NautilusFileUndoInfo *nautilus_file_undo_manager_get_action (void);

NautilusFileUndoManagerState nautilus_file_undo_manager_get_state (void);

void nautilus_file_undo_manager_undo (GtkWindow                      *parent_window,
                                      NautilusFileOperationsDBusData *dbus_data);
void nautilus_file_undo_manager_redo (GtkWindow                      *parent_window,
                                      NautilusFileOperationsDBusData *dbus_data);

gboolean nautilus_file_undo_manager_is_operating (void);
