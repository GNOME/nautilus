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
#include <libnautilus-extensions/nautilus-preferences.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>



struct NautilusTreeExpansionStateDetails {
	GHashTable *table;
};




static void               nautilus_tree_expansion_state_destroy          (GtkObject   *object);
static void               nautilus_tree_expansion_state_initialize       (gpointer     object,
									  gpointer     klass);
static void               nautilus_tree_expansion_state_initialize_class (gpointer     klass);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusTreeExpansionState, nautilus_tree_expansion_state, GTK_TYPE_OBJECT)


static gboolean	          expansion_table_hash_remove_func               (gpointer key,
									  gpointer value,
									  gpointer user_data);



/* infrastructure stuff */

static void
nautilus_tree_expansion_state_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_tree_expansion_state_destroy;

}


#define NAUTILUS_PREFERENCES_TREE_VIEW_EXPANSION_STATE		"tree-view/expansion_state"



static void
nautilus_tree_expansion_state_load_foreach_callback (char *uri,
						     NautilusTreeExpansionState *expansion_state)
{
	nautilus_tree_expansion_state_expand_node (expansion_state, uri);
}

static void 
nautilus_tree_expansion_state_load_table_from_gconf (NautilusTreeExpansionState *expansion_state)
{
	GSList *uris;

	uris = nautilus_preferences_get_string_list (NAUTILUS_PREFERENCES_TREE_VIEW_EXPANSION_STATE);

	g_slist_foreach (uris, (GFunc) nautilus_tree_expansion_state_load_foreach_callback, expansion_state);

	nautilus_g_slist_free_deep (uris);
}


static void
g_hash_table_get_keys_callback (gpointer key,
				gpointer value,
				gpointer user_data)
{
	GSList **keys;

	keys = (GSList **) user_data;
	*keys = g_slist_prepend (*keys, key);
}

static GSList *
g_hash_table_get_keys (GHashTable *hash_table)
{
	GSList *keys;

	keys = NULL;
	g_hash_table_foreach (hash_table,
			      g_hash_table_get_keys_callback,
			      &keys);
	return keys;
}


static void 
nautilus_tree_expansion_state_save_table_to_gconf (NautilusTreeExpansionState *expansion_state)
{
	GSList *uris;

	uris = g_hash_table_get_keys (expansion_state->details->table);

	nautilus_preferences_set_string_list (NAUTILUS_PREFERENCES_TREE_VIEW_EXPANSION_STATE, uris);

	g_slist_free (uris);
}

static void
nautilus_tree_expansion_state_initialize (gpointer object, gpointer klass)
{
	NautilusTreeExpansionState *expansion_state;
	
	expansion_state = NAUTILUS_TREE_EXPANSION_STATE (object);

	expansion_state->details = g_new0 (NautilusTreeExpansionStateDetails, 1);

	expansion_state->details->table = g_hash_table_new (g_str_hash, g_str_equal);
	
	nautilus_tree_expansion_state_load_table_from_gconf (expansion_state);
}


static void       
nautilus_tree_expansion_state_destroy (GtkObject *object)
{
	NautilusTreeExpansionState *expansion_state;

	expansion_state = (NautilusTreeExpansionState *) object;

	g_hash_table_foreach_remove (expansion_state->details->table,
				     expansion_table_hash_remove_func,
				     NULL);

	g_hash_table_destroy (expansion_state->details->table);

	g_free (expansion_state->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}


NautilusTreeExpansionState *
nautilus_tree_expansion_state_new ()
{
	return NAUTILUS_TREE_EXPANSION_STATE (gtk_type_new (NAUTILUS_TYPE_TREE_EXPANSION_STATE));
}


gboolean
nautilus_tree_expansion_state_is_node_expanded (NautilusTreeExpansionState *expansion_state,
						const char                 *uri)
{
	return (g_hash_table_lookup (expansion_state->details->table,
				     uri) != NULL);
}


void
nautilus_tree_expansion_state_expand_node (NautilusTreeExpansionState *expansion_state,
					   const char                 *uri)
{
	gpointer orig_key;
	gpointer value;

	if (!g_hash_table_lookup_extended (expansion_state->details->table,
					   uri,
					   &orig_key,
					   &value)) {
		g_hash_table_insert (expansion_state->details->table, g_strdup (uri), GINT_TO_POINTER (1));
	}
}

/* We only want to remember expanded nodes. */

void
nautilus_tree_expansion_state_collapse_node (NautilusTreeExpansionState *expansion_state,
					     const char                 *uri)
{
	nautilus_tree_expansion_state_remove_node (expansion_state, uri);
}

void
nautilus_tree_expansion_state_remove_node (NautilusTreeExpansionState *expansion_state,
					   const char                 *uri)
{
	gpointer orig_key;
	gpointer value;

	if (g_hash_table_lookup_extended (expansion_state->details->table,
					  uri,
					  &orig_key,
					  &value)) {
		g_hash_table_remove (expansion_state->details->table,
				     uri);
		g_free (orig_key);
	}
}

void
nautilus_tree_expansion_state_save (NautilusTreeExpansionState *expansion_state)
{
	nautilus_tree_expansion_state_save_table_to_gconf (expansion_state);
}

static gboolean	
expansion_table_hash_remove_func (gpointer key,
				  gpointer value,
				  gpointer user_data)
{
	g_free (key);
	return TRUE;
}




