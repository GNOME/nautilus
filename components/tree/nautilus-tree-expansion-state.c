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

#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-string.h>
#include <libnautilus-private/nautilus-global-preferences.h>

struct NautilusTreeExpansionStateDetails {
	GHashTable *table;
	GHashTable *ever_expanded_table;
};

static void               nautilus_tree_expansion_state_destroy          (GtkObject   *object);
static void               nautilus_tree_expansion_state_initialize       (gpointer     object,
									  gpointer     klass);
static void               nautilus_tree_expansion_state_initialize_class (gpointer     klass);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusTreeExpansionState, nautilus_tree_expansion_state, GTK_TYPE_OBJECT)


static gboolean	          expansion_table_hash_remove_func               (gpointer key,
									  gpointer value,
									  gpointer user_data);

static void           nautilus_tree_expansion_state_expand_node_internal (NautilusTreeExpansionState *expansion_state,
									  const char                 *uri);



/* infrastructure stuff */

static void
nautilus_tree_expansion_state_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_tree_expansion_state_destroy;

}

/* Preference name for the list of expanded uris */
#define NAUTILUS_PREFERENCES_TREE_VIEW_EXPANSION_STATE "tree-sidebar-panel/expansion_state"

static void
expansion_state_load_callback (const char *uri,
			       gpointer callback_data)
{
	g_return_if_fail (eel_strlen (uri) > 0);
	g_return_if_fail (NAUTILUS_IS_TREE_EXPANSION_STATE (callback_data));

	nautilus_tree_expansion_state_expand_node_internal
		(callback_data, uri);
}

static void 
nautilus_tree_expansion_state_load_from_preferences (NautilusTreeExpansionState *expansion_state)
{
	EelStringList *uris;

	uris = eel_preferences_get_string_list (NAUTILUS_PREFERENCES_TREE_VIEW_EXPANSION_STATE);

	eel_string_list_for_each (uris, expansion_state_load_callback, expansion_state);
	eel_string_list_free (uris);
}

static void
hash_table_get_keys_callback (gpointer key,
			      gpointer value,
			      gpointer user_data)
{
	GList **keys;

	keys = (GList **) user_data;
	*keys = g_list_prepend (*keys, key);
}

static GList *
hash_table_get_keys (GHashTable *hash_table)
{
	GList *keys;

	keys = NULL;
	g_hash_table_foreach (hash_table,
			      hash_table_get_keys_callback,
			      &keys);
	return keys;
}

static void 
nautilus_tree_expansion_state_save_to_preferences (NautilusTreeExpansionState *expansion_state)
{
	GList *glist;
	EelStringList *uris;

	glist = hash_table_get_keys (expansion_state->details->table);
	glist = eel_g_str_list_alphabetize (glist);

	uris = eel_string_list_new_from_g_list (glist, TRUE);
	eel_preferences_set_string_list (NAUTILUS_PREFERENCES_TREE_VIEW_EXPANSION_STATE, uris);
	g_list_free (glist);
	eel_string_list_free (uris);
}

static void
nautilus_tree_expansion_state_initialize (gpointer object, gpointer klass)
{
	NautilusTreeExpansionState *expansion_state;
	
	expansion_state = NAUTILUS_TREE_EXPANSION_STATE (object);

	expansion_state->details = g_new0 (NautilusTreeExpansionStateDetails, 1);

	expansion_state->details->table = g_hash_table_new (g_str_hash, g_str_equal);
	expansion_state->details->ever_expanded_table = g_hash_table_new (g_str_hash, g_str_equal);
	
	nautilus_tree_expansion_state_load_from_preferences (expansion_state);
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

	g_hash_table_foreach_remove (expansion_state->details->ever_expanded_table,
				     expansion_table_hash_remove_func,
				     NULL);
	g_hash_table_destroy (expansion_state->details->ever_expanded_table);

	g_free (expansion_state->details);
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}


NautilusTreeExpansionState *
nautilus_tree_expansion_state_new (void)
{
	NautilusTreeExpansionState *state;

	state = NAUTILUS_TREE_EXPANSION_STATE (gtk_object_new (NAUTILUS_TYPE_TREE_EXPANSION_STATE, NULL));
	gtk_object_ref (GTK_OBJECT (state));
	gtk_object_sink (GTK_OBJECT (state));
	return state;
}


gboolean
nautilus_tree_expansion_state_is_node_expanded (NautilusTreeExpansionState *expansion_state,
						const char                 *uri)
{
	return (g_hash_table_lookup (expansion_state->details->table,
				     uri) != NULL);
}


gboolean
nautilus_tree_expansion_state_was_ever_expanded (NautilusTreeExpansionState *expansion_state,
						 const char                 *uri)
{
	return (g_hash_table_lookup (expansion_state->details->ever_expanded_table,
				     uri) != NULL);
}


static void
nautilus_tree_expansion_state_expand_node_internal (NautilusTreeExpansionState *expansion_state,
						    const char                 *uri)
{
	gpointer orig_key;
	gpointer value;

	if (!g_hash_table_lookup_extended (expansion_state->details->table,
					   uri,
					   &orig_key,
					   &value)) {
		g_hash_table_insert (expansion_state->details->table, 
				     g_strdup (uri), 
				     GINT_TO_POINTER (1));
	}
}

void
nautilus_tree_expansion_state_expand_node (NautilusTreeExpansionState *expansion_state,
					   const char                 *uri)
{
	gpointer orig_key;
	gpointer value;

	nautilus_tree_expansion_state_expand_node_internal (expansion_state,
							    uri);

	if (!g_hash_table_lookup_extended (expansion_state->details->ever_expanded_table,
					   uri,
					   &orig_key,
					   &value)) {
		g_hash_table_insert (expansion_state->details->ever_expanded_table, 
				     g_strdup (uri), 
				     GINT_TO_POINTER (1));
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
	nautilus_tree_expansion_state_save_to_preferences (expansion_state);
}

static gboolean	
expansion_table_hash_remove_func (gpointer key,
				  gpointer value,
				  gpointer user_data)
{
	g_free (key);
	return TRUE;
}
