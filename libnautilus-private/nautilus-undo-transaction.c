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

#include "nautilus-undo-transaction.h"

/* nautilus_undo_transaction_new */
NautilusUndoTransaction *nautilus_undo_transaction_new (const gchar *name)
{
	NautilusUndoTransaction *transaction;

	transaction = g_new(NautilusUndoTransaction, 1);
	transaction->transaction_list = NULL;
	transaction->name = g_strdup(name);

	return transaction;
}

/* nautilus_undo_transaction_add_undoable */
gboolean nautilus_undo_transaction_add_undoable	(NautilusUndoTransaction *transaction, 
						 NautilusUndoable *undoable)
{	
	if (transaction == NULL) {
		g_warning("Cannot add undoable to a NULL transaction");
		return FALSE;
	}

	if (undoable == NULL) {
		g_warning("Cannot add a NULL undoable to a transaction");
		return FALSE;
	}

	transaction->transaction_list = g_list_append(transaction->transaction_list, undoable);
	
	return TRUE;
}



/* nautilus_undo_transaction_undo
 * Parse transaction and send undo signals to undoable objects stored in transaction 
 */

gboolean 
nautilus_undo_transaction_undo (NautilusUndoTransaction *transaction)
{
	NautilusUndoable *undoable;
	guint index;

	g_return_val_if_fail(transaction != NULL, FALSE);
		
	for ( index = 0; index < g_list_length(transaction->transaction_list); index++) {

		/* Get pointer to undoable */
		undoable = g_list_nth_data(transaction->transaction_list, index);

		/* Send object restore from undo signal */
		if (undoable != NULL)
			nautilus_undoable_restore_from_undo_snapshot (undoable);
	}

	return TRUE;
}

const gchar *nautilus_undo_transaction_get_name	(NautilusUndoTransaction *transaction)
{
	g_return_val_if_fail(transaction != NULL, NULL);
	
	return transaction->name;
}



