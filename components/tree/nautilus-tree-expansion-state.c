/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 */

/* nautilus-tree-expansion-state.c - class to track expansion state for the tree view */

#include <config.h>
#include "nautilus-tree-expansion-state.h"

#include <libnautilus-extensions/nautilus-gtk-macros.h>




struct NautilusTreeExpansionStateDetails {
};




static void               nautilus_tree_expansion_state_destroy          (GtkObject   *object);
static void               nautilus_tree_expansion_state_initialize       (gpointer     object,
								gpointer     klass);
static void               nautilus_tree_expansion_state_initialize_class (gpointer     klass);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusTreeExpansionState, nautilus_tree_expansion_state, GTK_TYPE_OBJECT)




/* infrastructure stuff */

static void
nautilus_tree_expansion_state_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_tree_expansion_state_destroy;

}

static void
nautilus_tree_expansion_state_initialize (gpointer object, gpointer klass)
{
	NautilusTreeExpansionState *expansion_state;

	expansion_state = NAUTILUS_TREE_EXPANSION_STATE (object);

	expansion_state->details = g_new0 (NautilusTreeExpansionStateDetails, 1);
}

static void       
nautilus_tree_expansion_state_destroy (GtkObject *object)
{
	NautilusTreeExpansionState *expansion_state;

	expansion_state = (NautilusTreeExpansionState *) object;

	g_free (expansion_state->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}


NautilusTreeExpansionState *
nautilus_tree_expansion_state_new ()
{
	return NAUTILUS_TREE_EXPANSION_STATE (gtk_type_new (NAUTILUS_TYPE_TREE_EXPANSION_STATE));
}

