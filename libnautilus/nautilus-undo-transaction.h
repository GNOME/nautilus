/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* NautilusUndoTransaction - An object for an undoable transcation.
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

#ifndef NAUTILUS_UNDO_TRANSACTION_H
#define NAUTILUS_UNDO_TRANSACTION_H

#include <glib.h>
#include <bonobo/bonobo-object.h>
#include "nautilus-undoable.h"
#include "nautilus-undo.h"

#define NAUTILUS_TYPE_UNDO_TRANSACTION \
	(nautilus_undo_transaction_get_type ())
#define NAUTILUS_UNDO_TRANSACTION(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_UNDO_TRANSACTION, NautilusUndoTransaction))
#define NAUTILUS_UNDO_TRANSACTION_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_UNDO_TRANSACTION, NautilusUndoTransactionClass))
#define NAUTILUS_IS_UNDO_TRANSACTION(obj) \
        (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_UNDO_TRANSACTION))
#define NAUTILUS_IS_UNDO_TRANSACTION_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass),	NAUTILUS_TYPE_UNDO_TRANSACTION))

typedef struct NautilusUndoTransactionClass NautilusUndoTransactionClass;

	
struct NautilusUndoTransaction {
	BonoboObject parent;
	gchar *name;
	GList *transaction_list;
};

struct NautilusUndoTransactionClass {
	BonoboObjectClass parent_class;	
	gpointer servant_init_func, servant_destroy_func, vepv;
};

GtkType			nautilus_undo_transaction_get_type 	  (void);
NautilusUndoTransaction *nautilus_undo_transaction_new		  (const gchar *name);

gboolean		nautilus_undo_transaction_add_undoable	  (NautilusUndoTransaction *transaction, 
								   NautilusUndoable *undoable);
const gchar 		*nautilus_undo_transaction_get_name	  (NautilusUndoTransaction *transaction);
gboolean		nautilus_undo_transaction_contains_object (NautilusUndoTransaction *transaction,
								   GtkObject *object);
#endif
