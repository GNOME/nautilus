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

#include <bonobo/bonobo-control.h>

#include "nautilus-undo-transaction.h"
#include "nautilus-undo.h"

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
	BonoboObject parent;
	NautilusUndoManagerDetails *details;
};

struct NautilusUndoManagerClass {
	BonoboObjectClass parent_class;	
	void (* undo_transaction_occurred) (GtkObject *object, gpointer data);
	gpointer servant_init_func, servant_destroy_func, vepv;
};


/* GtkObject */
GtkType               nautilus_undo_manager_get_type          (void);
NautilusUndoManager 	*nautilus_undo_manager_new               (void);

/* Prototypes */
gboolean              nautilus_undo_manager_can_undo          (NautilusUndoManager   *manager);
gboolean              nautilus_undo_manager_can_redo          (NautilusUndoManager   *manager);
void                  nautilus_undo_manager_enable_redo       (NautilusUndoManager   *manager,
							       gboolean               value);
void                  nautilus_undo_manager_set_queue_depth   (NautilusUndoManager   *manager,
							       gint                   depth);
							       
void                  nautilus_attach_undo_manager            (GtkObject             *object,
							       Nautilus_Undo_Manager  manager);
void                  nautilus_share_undo_manager             (GtkObject             *destination_object,
							       GtkObject             *source_object);
void                  nautilus_undo_set_up_bonobo_control     (BonoboControl         *control);
Nautilus_Undo_Manager nautilus_get_undo_manager               (GtkObject             *start_object);

void		      nautilus_undo_manager_stash_global_undo (Nautilus_Undo_Manager undo_manager);
Nautilus_Undo_Manager nautilus_undo_manager_get_global_undo   (void);


#endif
