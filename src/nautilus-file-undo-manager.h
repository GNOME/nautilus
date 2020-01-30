
/* nautilus-file-undo-manager.h - Manages the undo/redo stack
 *
 * Copyright (C) 2007-2011 Amos Brocco
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
 * Author: Amos Brocco <amos.brocco@gmail.com>
 */

#pragma once

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include "nautilus-file-undo-operations.h"

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
