/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* NautilusUndoContext - Used internally by undo machinery.
 *                       Not public.
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

#ifndef NAUTILUS_UNDO_CONTEXT_H
#define NAUTILUS_UNDO_CONTEXT_H

#include <bonobo/bonobo-object.h>
#include <libnautilus/nautilus-distributed-undo.h>

#define NAUTILUS_TYPE_UNDO_CONTEXT \
	(nautilus_undo_context_get_type ())
#define NAUTILUS_UNDO_CONTEXT(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_UNDO_CONTEXT, NautilusUndoContext))
#define NAUTILUS_UNDO_CONTEXT_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_UNDO_CONTEXT, NautilusUndoContextClass))
#define NAUTILUS_IS_UNDO_CONTEXT(obj) \
        (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_UNDO_CONTEXT))
#define NAUTILUS_IS_UNDO_CONTEXT_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass),	NAUTILUS_TYPE_UNDO_CONTEXT))

typedef struct {
	BonoboObject parent_slot;
	Nautilus_Undo_Manager undo_manager;
} NautilusUndoContext;

typedef struct {
	BonoboObjectClass parent_slot;
} NautilusUndoContextClass;

GtkType              nautilus_undo_context_get_type (void);
NautilusUndoContext *nautilus_undo_context_new      (Nautilus_Undo_Manager undo_manager);

#endif /* NAUTILUS_UNDO_CONTEXT_H */
