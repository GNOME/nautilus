/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* NautilusUndoManager - Manages undo and redo transactions.
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Author: Gene Z. Ragan <gzr@eazel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef NAUTILUS_UNDO_MANAGER_H
#define NAUTILUS_UNDO_MANAGER_H

#include <bonobo/bonobo-object.h>
#include <libnautilus/nautilus-undo-manager-component.h>
#include <bonobo/bonobo-persist.h>
#include "nautilus-undo-transaction.h"

#define NAUTILUS_TYPE_UNDO_MANAGER \
	(nautilus_undo_manager_get_type ())
#define NAUTILUS_UNDO_MANAGER(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_UNDO_MANAGER, NautilusUndoManager))
#define NAUTILUS_UNDO_MANAGER_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_UNDO_MANAGER, NautilusUndoManagerClass))
#define NAUTILUS_IS_UNDO_MANAGER(obj) \
        (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_UNDO_MANAGER))
#define NAUTILUS_IS_UNDO_MANAGER_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass),	NAUTILUS_TYPE_UNDO_MANAGER))
	
typedef struct NautilusUndoManagerClass NautilusUndoManagerClass;
typedef struct NautilusUndoManagerDetails NautilusUndoManagerDetails;

struct NautilusUndoManager {
	BonoboPersist parent;
	NautilusUndoManagerDetails *details;
};

struct NautilusUndoManagerClass {
	BonoboPersistClass parent_class;
	
	void (* undo_transaction_occurred) (GtkObject *object, gpointer data);

	gpointer servant_init_func, servant_destroy_func, vepv;
};


/* GtkObject */
GtkType			nautilus_undo_manager_get_type 				(void);
NautilusUndoManager 	*nautilus_undo_manager_new 				(void);

/* Prototypes */
gboolean 		nautilus_undo_manager_initialize_global_manager		 (void);
gboolean 		nautilus_undo_manager_add_undoable_to_transaction 	 (NautilusUndoable *undoable);
gboolean		nautilus_undo_manager_begin_transaction			 (const gchar *name);
gboolean 		nautilus_undo_manager_end_transaction			 (void);
gboolean 		nautilus_undo_manager_undo_last_transaction		 (void);
gboolean 		nautilus_undo_manager_redo_last_undone_transaction	 (void);
NautilusUndoTransaction *nautilus_undo_manager_get_current_transaction 	 	 (void);
gboolean 		nautilus_undo_manager_transaction_in_progress 		 (void);
gboolean 		nautilus_undo_manager_add_undo_transaction 		 (NautilusUndoTransaction 
										  *transaction);
gboolean 		nautilus_undo_manager_add_redo_transaction 		 (NautilusUndoTransaction
										  *transaction);
gboolean 		nautilus_undo_manager_remove_transaction 		 (NautilusUndoTransaction 
										  *transaction);
gboolean 		nautilus_undo_manager_unregister_object 		 (GtkObject *object);
gboolean 		nautilus_undo_manager_can_undo 		 		 (void);
gboolean 		nautilus_undo_manager_can_redo 		 		 (void);
gint			nautilus_undo_manager_get_current_undo_transaction_name	 (gchar *name, gint max_length);
gint			nautilus_undo_manager_get_current_redo_transaction_name	 (gchar *name, gint max_length);
NautilusUndoManager	*nautilus_undo_manager_get_undo_manager	 		 (void);
gboolean		nautilus_undo_manager_enable_redo 			 (gboolean value);
gboolean		nautilus_undo_manager_set_queue_depth 			 (gint depth);

#endif

