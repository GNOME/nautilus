/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-preferences.h - Preference peek/poke/notify object implementation.

   Copyright (C) 1999, 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include <string.h>

#include "nautilus-preferences.h"

#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-string.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

static const char PREFERENCES_GCONF_PATH[] = "/nautilus";

/*
 * PreferencesHashNode:
 *
 * A structure to manage preference hash table nodes.
 * Preferences are hash tables.  The hash key is the preference name
 * (a string).  The  hash value is a pointer of the following struct:
 */
typedef struct {
	NautilusPreference	*preference;
	GList			*callback_list;
	int			gconf_connection;
} PreferencesHashNode;

/*
 * PreferencesCallbackNode:
 *
 * A structure to manage callback lists.  A callback list is a GList.
 * The callback_data in each list node is a pointer to the following 
 * struct:
 */
typedef struct {
	NautilusPreferencesCallback	callback_proc;
	gpointer			callback_data;
	const PreferencesHashNode	*hash_node;
} PreferencesCallbackNode;

/*
 * PreferencesGlobalData
 *
 * A structure to make it easier to grok the usage of global variables
 * in the code below.
 */
typedef struct {
	GHashTable	  *preference_table;
	GConfClient	  *gconf_client;
} PreferencesGlobalData;

static PreferencesGlobalData GLOBAL = { NULL, NULL };

/* PreferencesHashNode functions */
static PreferencesHashNode *    preferences_hash_node_alloc                    (char                        *name,
										char                        *description,
										NautilusPreferenceType       type,
										gconstpointer                default_value,
										gpointer                     data);
static void                     preferences_hash_node_free                     (PreferencesHashNode         *node);
static void                     preferences_hash_node_free_func                (gpointer                     key,
										gpointer                     value,
										gpointer                     callback_data);




/* PreferencesCallbackNode functions */
static PreferencesCallbackNode *preferneces_callback_node_alloc                (NautilusPreferencesCallback  callback_proc,
										gpointer                     callback_data,
										const PreferencesHashNode   *hash_node);
static void                     preferneces_callback_node_free                 (PreferencesCallbackNode     *node);
static void                     preferneces_callback_node_free_func            (gpointer                     data,
										gpointer                     callback_data);
static void                     preferneces_callback_node_invoke_func          (gpointer                     data,
										gpointer                     callback_data);
static void                     preferences_hash_node_add_callback             (PreferencesHashNode         *node,
										NautilusPreferencesCallback  callback_proc,
										gpointer                     callback_data);
static void                     preferences_hash_node_remove_callback          (PreferencesHashNode         *node,
										NautilusPreferencesCallback  callback_proc,
										gpointer                     callback_data);




/* Private stuff */
PreferencesHashNode *           preferences_hash_node_lookup                   (const char                  *name);
PreferencesHashNode *           preferences_hash_node_lookup_with_registration (const char                  *pref_name,
										NautilusPreferenceType       pref_type,
										gconstpointer                default_value);
static void                     preferences_register                           (char                        *name,
										char                        *description,
										NautilusPreferenceType       type,
										gconstpointer                default_value,
										gpointer                     data);
static void                     set_default_value_if_needed                    (const char                  *name,
										NautilusPreferenceType       type,
										gconstpointer                default_value);



/* Gconf callbacks */
static void                     preferences_gconf_callback                     (GConfClient                 *client,
										guint                        cnxn_id,
										const gchar                 *key,
										GConfValue                  *value,
										gboolean                     is_default,
										gpointer                     user_data);

/**
 * preferences_hash_node_alloc
 *
 * Allocate a preference hash node.
 * @info: Pointer to info structure to use for the node memebers.
 *
 * Return value: A newly allocated node.
 **/
static PreferencesHashNode *
preferences_hash_node_alloc (char			*name,
			     char			*description,
			     NautilusPreferenceType	type,
			     gconstpointer		default_value,
			     gpointer			data)
{
	PreferencesHashNode * node;
	
	g_assert (name != NULL);

	node = g_new (PreferencesHashNode, 1);

 	node->preference = NAUTILUS_PREFERENCE (nautilus_preference_new_from_type (name, type));

 	g_assert (node->preference != NULL);

	if (description) {
		nautilus_preference_set_description (node->preference, description);
	}

	set_default_value_if_needed (name, type, default_value);

	node->callback_list = NULL;

	node->gconf_connection = 0;

	return node;
}

/**
 * preferences_hash_node_free
 *
 * Free a preference hash node members along with the node itself.
 * @preferences_hash_node: The node to free.
 **/
static void
preferences_hash_node_free (PreferencesHashNode *node)
{
	g_assert (node != NULL);

	g_assert (node->preference != NULL);

	/* Remove the gconf notification if its still lingering */
	if (node->gconf_connection != 0)
	{
		gconf_client_notify_remove (GLOBAL.gconf_client,
					    node->gconf_connection);

		node->gconf_connection = 0;
	}

	nautilus_g_list_free_deep_custom (node->callback_list,
					  preferneces_callback_node_free_func,
					  NULL);
	
	gtk_object_unref (GTK_OBJECT (node->preference));
	node->preference = NULL;

	node->callback_list = NULL;

	g_free (node);
}

/**
 * preferences_hash_node_add_callback
 *
 * Add a callback to a pref node.  Callbacks are fired whenever
 * the pref value changes.
 * @preferences_hash_node: The hash node.
 * @callback_proc: The user supplied callback.
 * @callback_data: The user supplied closure.
 **/
static void
preferences_hash_node_add_callback (PreferencesHashNode		*node,
				    NautilusPreferencesCallback	callback_proc,
				    gpointer			callback_data)
{
	PreferencesCallbackNode	*preferneces_callback_node;

	g_assert (node != NULL);

	g_assert (callback_proc != NULL);

	preferneces_callback_node = preferneces_callback_node_alloc (callback_proc, 
								     callback_data,
								     node);
	
	g_assert (preferneces_callback_node != NULL);
	
	node->callback_list = g_list_append (node->callback_list, 
					     (gpointer) preferneces_callback_node);

	/*
	 * We install only one gconf notification for each preference node.
	 * Otherwise, we would invoke the installed callbacks more than once
	 * per registered callback.
	 */
	if (node->gconf_connection == 0) {
		char *name = nautilus_preference_get_name (node->preference);
		
		g_assert (name != NULL);
		
		/*
		 * Ref the preference here, cause we use for the gconf callback data.
		 * See preferences_hash_node_remove_callback() to make sure the ref is balanced.
		 */
		g_assert (node->preference != NULL);
		gtk_object_ref (GTK_OBJECT (node->preference));
		
		g_assert (node->gconf_connection == 0);
		
		node->gconf_connection = gconf_client_notify_add (GLOBAL.gconf_client,
								  name,
								  preferences_gconf_callback,
								  node->preference,
								  NULL,
								  NULL);
		g_free (name);
	}
}

/**
 * preferences_hash_node_remove_callback
 *
 * remove a callback from a pref node.  Both the callback and the callback_data must
 * match in order for a callback to be removed from the node.
 * @preferences_hash_node: The hash node.
 * @callback_proc: The user supplied callback.
 * @callback_data: The user supplied closure.
 **/
static void
preferences_hash_node_remove_callback (PreferencesHashNode		*node,
				       NautilusPreferencesCallback	callback_proc,
				       gpointer				callback_data)
{
	GList		 *new_list;
	GList		 *iterator;

	g_assert (node != NULL);

	g_assert (callback_proc != NULL);

	g_assert (node->callback_list != NULL);
	
	new_list = g_list_copy (node->callback_list);
	
	for (iterator = new_list; iterator != NULL; iterator = iterator->next) {
		PreferencesCallbackNode *callback_info = (PreferencesCallbackNode *) iterator->data;
		
		g_assert (callback_info != NULL);
		
		if (callback_info->callback_proc == callback_proc &&
		    callback_info->callback_data == callback_data) {
			node->callback_list = g_list_remove (node->callback_list, 
							     (gpointer) callback_info);
			
			preferneces_callback_node_free (callback_info);
		}
	}
	
	/*
	 * If there are no callbacks left in the node, remove the gconf 
	 * notification as well.
	 */
	if (node->callback_list == NULL) {
		g_assert (node->gconf_connection != 0);
		
		gconf_client_notify_remove (GLOBAL.gconf_client,
					    node->gconf_connection);
		
		node->gconf_connection = 0;
		
		/*
		 * Unref the preference here to balance the ref added in 
		 * preferences_hash_node_add_callback().
		 */
		g_assert (node->preference != NULL);
		gtk_object_unref (GTK_OBJECT (node->preference));
	}
}

/**
 * preferences_hash_node_free_func
 *
 * A function that frees a pref hash node.  It is meant to be fed to 
 * g_hash_table_foreach ()
 * @key: The hash key privately maintained by the GHashTable.
 * @value: The hash value privately maintained by the GHashTable.
 * @callback_data: The callback_data privately maintained by the GHashTable.
 **/
static void
preferences_hash_node_free_func (gpointer key,
				 gpointer value,
				 gpointer callback_data)
{
	g_assert (value != NULL);

	preferences_hash_node_free ((PreferencesHashNode *) value);
}

/**
 * preferneces_callback_node_alloc
 *
 * Allocate a callback info struct from the given values.  PreferencesCallbackNode
 * structures are used as nodes for the callbac_list member of pref hash table
 * nodes.
 * @callback_proc: The callback.
 * @callback_data: The user data.
 * @hash_node: The hash table node this callback belongs to.
 *
 * Return value: A newly allocated node.
 **/
static PreferencesCallbackNode *
preferneces_callback_node_alloc (NautilusPreferencesCallback	callback_proc,
				 gpointer			callback_data,
				 const PreferencesHashNode	*hash_node)
{
	PreferencesCallbackNode *callback_node;
	
	g_assert (callback_proc != NULL);

	callback_node = g_new (PreferencesCallbackNode, 1);

	callback_node->callback_proc = callback_proc;
	callback_node->callback_data = callback_data;
	callback_node->hash_node = hash_node;

	return callback_node;
}

/**
 * preferneces_callback_node_free
 *
 * Free a callback info struct.
 * @preferneces_callback_node: The struct to free.
 **/
static void
preferneces_callback_node_free (PreferencesCallbackNode *callback_node)
{
	g_assert (callback_node != NULL);

	callback_node->callback_proc = NULL;
	callback_node->callback_data = NULL;

	g_free (callback_node);
}

/**
 * preferneces_callback_node_free_func
 *
 * A function that frees a callback info struct.  It is meant to be fed to 
 * g_list_foreach ()
 * @data: The list data privately maintained by the GList.
 * @callback_data: The callback_data privately maintained by the GList.
 **/
static void
preferneces_callback_node_free_func (gpointer	data,
				     gpointer	callback_data)
{
	g_assert (data != NULL);

	preferneces_callback_node_free ((PreferencesCallbackNode *) data);
}

/**
 * preferneces_callback_node_invoke_func
 *
 * A function that invokes a callback from the given struct.  It is meant to be fed to 
 * g_list_foreach ()
 * @data: The list data privately maintained by the GList.
 * @callback_data: The callback_data privately maintained by the GList.
 **/
static void
preferneces_callback_node_invoke_func (gpointer	data,
				       gpointer	callback_data)
{
	PreferencesCallbackNode	*callback_node;
	char			*preference_name;

	callback_node = (PreferencesCallbackNode *) data;

	g_assert (callback_node != NULL);
	g_assert (callback_node->callback_proc != NULL);

	preference_name = nautilus_preference_get_name (callback_node->hash_node->preference);

	g_assert (preference_name != NULL);

 	(* callback_node->callback_proc) (callback_node->callback_data);

	g_free (preference_name);
}

static void
preferences_register (char			*name,
		      char			*description,
		      NautilusPreferenceType	type,
		      gconstpointer		default_value,
		      gpointer			data)
{
	PreferencesHashNode *node;

	g_return_if_fail (nautilus_preferences_is_initialized ());

	g_return_if_fail (name != NULL);
	g_return_if_fail (description != NULL);
	
	node = preferences_hash_node_lookup (name);

	if (node) {
		g_warning ("the '%s' preference is already registered", name);
		return;
	}

	node = preferences_hash_node_alloc (name, description, type, default_value, data);

	g_hash_table_insert (GLOBAL.preference_table, (gpointer) name, (gpointer) node);

	g_assert (node->preference != NULL);
}

/**
 * nautilus_preferences_find_preference
 *
 * Search for a named preference in the given preferences and return it.
 * @preferences: The preferences to search
 *
 * Return value: A referenced pointer to the preference object that corresponds
 * to the given preference name.  The caller should gtk_object_unref() the return
 * value of this function.
 **/
NautilusPreference *
nautilus_preferences_find_preference (const char *name)
{
	PreferencesHashNode *node;

	g_return_val_if_fail (nautilus_preferences_is_initialized (), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	node = preferences_hash_node_lookup (name);
	
	g_assert (node != NULL);

	gtk_object_ref (GTK_OBJECT (node->preference));

	return node->preference;
}

void
nautilus_preferences_set_info (const char		*name,
			       const char		*description,
			       NautilusPreferenceType	type,
			       gconstpointer		default_value)
{
	PreferencesHashNode *node;

	g_return_if_fail (nautilus_preferences_is_initialized ());
	g_return_if_fail (name != NULL);

	node = preferences_hash_node_lookup_with_registration (name, type, default_value);
	
	g_assert (node != NULL);
	g_assert (node->preference != NULL);

	if (description) {
		nautilus_preference_set_description (node->preference, description);
	}

	set_default_value_if_needed (name, type, default_value);
}

/**
 * set_default_value_if_needed
 *
 * This function will ask gconf for a value. If
 * 
 * The value is not found in the user's database:
 *   It will be added to the database using the given default value.  
 *
 * The value is found in the user's database:
 *   Nothing.
 *
 * @name: The name of the preference.
 * @type: The type of preference.
 * @default_value: The default_value to use.
 **/
static void
set_default_value_if_needed (const char			*name,
			     NautilusPreferenceType	type,
			     gconstpointer		default_value)
{
	GConfValue *value = NULL;

	g_return_if_fail (nautilus_preferences_is_initialized ());
	g_return_if_fail (name != NULL);

	/* Find out if the preference exists at all */
	value = gconf_client_get_without_default (GLOBAL.gconf_client,
						  name,
						  NULL);

	/* The value does not exist, so create one */
	if (!value) {
		switch (type)
		{
		case NAUTILUS_PREFERENCE_STRING: 
			/* Gconf will not grok NULL strings, so for this case dont do it. */
			if (default_value) {
				value = gconf_value_new (GCONF_VALUE_STRING);
				gconf_value_set_string (value, (const char *) default_value);
			}
			break;
		case NAUTILUS_PREFERENCE_BOOLEAN:
			value = gconf_value_new (GCONF_VALUE_BOOL);
			gconf_value_set_bool (value, GPOINTER_TO_INT (default_value));
			break;
		case NAUTILUS_PREFERENCE_ENUM:
			value = gconf_value_new (GCONF_VALUE_INT);
			gconf_value_set_int (value, GPOINTER_TO_INT (default_value));
			break;
		}
		
		if (value) {
			gconf_client_set (GLOBAL.gconf_client, name, value, NULL);
		}
	}

	if (value) {
		gconf_value_destroy (value);
	}
}

void
nautilus_preferences_enum_add_entry (const char	*name,
				     const char	*entry_name,
				     const char	*entry_description,
				     int	entry_value)
{
	PreferencesHashNode *node;
	
	g_return_if_fail (nautilus_preferences_is_initialized ());
	g_return_if_fail (name != NULL);
	
	node = preferences_hash_node_lookup_with_registration (name, NAUTILUS_PREFERENCE_ENUM, NULL);
	
	g_assert (node != NULL);
	g_assert (node->preference != NULL);

	g_assert (nautilus_preference_get_preference_type (node->preference) == NAUTILUS_PREFERENCE_ENUM);

	nautilus_preference_enum_add_entry (node->preference,
					    entry_name,
					    entry_description,
					    entry_value);
}

PreferencesHashNode *
preferences_hash_node_lookup (const char *name)
{
	gpointer hash_value;

	g_assert (nautilus_preferences_is_initialized ());
	g_assert (name != NULL);

	hash_value = g_hash_table_lookup (GLOBAL.preference_table, (gconstpointer) name);
	
	return (PreferencesHashNode *) hash_value;
}

PreferencesHashNode *
preferences_hash_node_lookup_with_registration (const char		*name,
					      NautilusPreferenceType	type,
					      gconstpointer             default_value)
{
	PreferencesHashNode * node;

	g_assert (nautilus_preferences_is_initialized ());
	g_assert (name != NULL);

	node = preferences_hash_node_lookup (name);

	if (!node) {
		preferences_register (g_strdup (name),
				      "Unspecified Description",
				      type,						   
				      default_value,
				      (gpointer) NULL);
		
		node = preferences_hash_node_lookup (name);
	}
	
	g_assert (node != NULL);

	return node;
}

static void
preferences_gconf_callback (GConfClient	*client, 
			    guint	connection_id, 
			    const gchar	*key, 
			    GConfValue	*value, 
			    gboolean	is_default, 
			    gpointer	user_data)
{
	PreferencesHashNode	*node;
	NautilusPreference	*expected_preference;
	char			*expected_key;

	g_assert (nautilus_preferences_is_initialized ());
	g_assert (key != NULL);

	g_assert (user_data != NULL);
	g_assert (NAUTILUS_IS_PREFERENCE (user_data));

	expected_preference = NAUTILUS_PREFERENCE (user_data);

	/* 
	 * This gconf notification was installed with an expected key in mind.
	 * The expected is not always the key passed into this function.
	 *
	 * This happens when the expected key is a namespace, and the key
	 * that changes is actually beneath that namespace.
	 *
	 * For example:
	 *
	 * 1. Tell me when "foo/bar" changes.
	 * 2. "foo/bar/x" or "foo/bar/y" or "foo/bar/z" changes
	 * 3. I get notified that "foo/bar/{x,y,z}" changed - not "foo/bar"
	 *
	 * This makes sense, since it is "foo/bar/{x,y,z}" that indeed changed.
	 *
	 * So we can use this mechanism to keep track of changes within a whole
	 * namespace by comparing the expected_key to the given key.
	 */
	expected_key = nautilus_preference_get_name (expected_preference);
	
	g_assert (expected_key != NULL);

	if (strcmp (key, expected_key) != 0) {
		/* The prefix should be the same */
		g_assert (strncmp (key, expected_key, strlen (expected_key)) == 0);
		key = expected_key;
	}
	
	g_assert (key != NULL);
	
	node = preferences_hash_node_lookup (key);
	
	g_assert (node != NULL);
	g_assert (node->preference != NULL);

	gconf_client_suggest_sync (GLOBAL.gconf_client, NULL);

	/* Invoke callbacks for this node */
	if (node->callback_list) {
		g_list_foreach (node->callback_list,
				preferneces_callback_node_invoke_func,
				(gpointer) NULL);
	}

	g_free (expected_key);
}

gboolean
nautilus_preferences_add_boolean_callback (const char			*name,
					   NautilusPreferencesCallback	callback_proc,
					   gpointer			callback_data)
{
	PreferencesHashNode	*node;

	g_return_val_if_fail (nautilus_preferences_is_initialized (), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (callback_proc != NULL, FALSE);

	node = preferences_hash_node_lookup_with_registration (name, NAUTILUS_PREFERENCE_BOOLEAN, NULL);

	if (node == NULL) {
		g_warning ("trying to add a callback for an unregistered preference");
		return FALSE;
	}

	preferences_hash_node_add_callback (node, callback_proc, callback_data);

	return TRUE;
}					

gboolean
nautilus_preferences_add_enum_callback (const char			*name,
					NautilusPreferencesCallback	callback_proc,
					gpointer			callback_data)
{
	PreferencesHashNode *node;

	g_return_val_if_fail (nautilus_preferences_is_initialized (), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (callback_proc != NULL, FALSE);

	node = preferences_hash_node_lookup_with_registration (name, NAUTILUS_PREFERENCE_ENUM, NULL);

	if (node == NULL) {
		g_warning ("trying to add a callback for an unregistered preference");
		return FALSE;
	}

	preferences_hash_node_add_callback (node, callback_proc, callback_data);

	return TRUE;
}

gboolean
nautilus_preferences_add_callback (const char			*name,
				   NautilusPreferencesCallback	callback_proc,
				   gpointer			callback_data)
{
	PreferencesHashNode *node;

	g_return_val_if_fail (nautilus_preferences_is_initialized (), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (callback_proc != NULL, FALSE);

	node = preferences_hash_node_lookup_with_registration (name, NAUTILUS_PREFERENCE_STRING, NULL);

	if (node == NULL) {
		g_warning ("trying to add a callback for an unregistered preference");
		return FALSE;
	}

	preferences_hash_node_add_callback (node, callback_proc, callback_data);

	return TRUE;
}

gboolean
nautilus_preferences_remove_callback (const char		   *name,
				      NautilusPreferencesCallback  callback_proc,
				      gpointer			   callback_data)
{
	PreferencesHashNode *node;

	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (callback_proc != NULL, FALSE);

	node = preferences_hash_node_lookup (name);
	if (node == NULL) {
		g_warning ("trying to remove a callback for an unregistered preference");
		return FALSE;
	}

	preferences_hash_node_remove_callback (node, callback_proc, callback_data);

	return TRUE;
}

void
nautilus_preferences_set_boolean (const char	*name,
				  gboolean	boolean_value)
{
	gboolean gconf_result;

	g_return_if_fail (nautilus_preferences_is_initialized ());
	g_return_if_fail (name != NULL);

	/* Make sure the preference value is indeed different */
	if (gconf_client_get_bool (GLOBAL.gconf_client, name, NULL) == boolean_value) {
		return;
	}

	gconf_result = gconf_client_set_bool (GLOBAL.gconf_client, name, boolean_value, NULL);

	g_assert (gconf_result);

	gconf_client_suggest_sync (GLOBAL.gconf_client, NULL);
}

gboolean
nautilus_preferences_get_boolean (const char	*name,
				  gboolean	default_value)
{
	g_return_val_if_fail (nautilus_preferences_is_initialized (), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	return gconf_client_get_bool (GLOBAL.gconf_client, name, NULL);
}

void
nautilus_preferences_set_enum (const char    *name,
			       int           enum_value)
{
	gboolean gconf_result;

	g_return_if_fail (nautilus_preferences_is_initialized ());
	g_return_if_fail (name != NULL);

	/* Make sure the preference value is indeed different */
	if (gconf_client_get_int (GLOBAL.gconf_client, name, NULL) == enum_value) {
		return;
	}

	gconf_result = gconf_client_set_int (GLOBAL.gconf_client, name, enum_value, NULL);

	g_assert (gconf_result);

	gconf_client_suggest_sync (GLOBAL.gconf_client, NULL);
}

int
nautilus_preferences_get_enum (const char	*name,
			       int		default_value)
{
	g_return_val_if_fail (nautilus_preferences_is_initialized (), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	return gconf_client_get_int (GLOBAL.gconf_client, name, NULL);
}

void
nautilus_preferences_set (const char *name, 
			  const char *value)
{
	gboolean gconf_result;

	g_return_if_fail (nautilus_preferences_is_initialized ());
	g_return_if_fail (name != NULL);

	/* Make sure the preference value is indeed different */
	if (value) {
		char *current_value = gconf_client_get_string (GLOBAL.gconf_client, name, NULL);
		int result = nautilus_strcmp (current_value, value);
		
		if (current_value) {
			g_free (current_value);
		}

		if (result == 0) {
			return;
		}
	}

	gconf_result = gconf_client_set_string (GLOBAL.gconf_client, name, value, NULL);

	g_assert (gconf_result);

	gconf_client_suggest_sync (GLOBAL.gconf_client, NULL);
}

char *
nautilus_preferences_get (const char	*name,
			  const char	*default_value)
{
	gchar *value = NULL;

	g_return_val_if_fail (nautilus_preferences_is_initialized (), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	value = gconf_client_get_string (GLOBAL.gconf_client, name, NULL);

	if (!value && default_value) {
		value = g_strdup (default_value);
	}
	
	return value;
}

gboolean
nautilus_preferences_initialize (int				argc, 
				 char				**argv,
				 guint				num_user_levels,
				 const NautilusStringList	*user_level_names)
{
	GConfError *error = NULL;

	g_return_val_if_fail (num_user_levels > 0, FALSE);
	g_return_val_if_fail (user_level_names != NULL, FALSE);
	g_return_val_if_fail (nautilus_string_list_get_length (user_level_names) == num_user_levels, FALSE);
	
	if (!gconf_init (argc, argv, &error)) {
		g_assert (error != NULL);

		/* FIXME bugzilla.eazel.com 672: Need better error reporting here */
		g_warning ("GConf init failed:\n  %s", error->str);
		
		gconf_error_destroy (error);
		
		error = NULL;
		
		return FALSE;
	}

	g_assert (GLOBAL.preference_table == NULL);
	g_assert (GLOBAL.gconf_client == NULL);

	GLOBAL.preference_table = g_hash_table_new (g_str_hash, g_str_equal);

	g_assert (GLOBAL.preference_table != NULL);

	GLOBAL.gconf_client = gconf_client_new ();

	g_assert (GLOBAL.gconf_client != NULL);

	/* Let gconf know about ~/.gconf/nautilus so that callbacks work */
	gconf_client_add_dir (GLOBAL.gconf_client,
			      PREFERENCES_GCONF_PATH,
			      GCONF_CLIENT_PRELOAD_RECURSIVE,
			      NULL);
	
	return TRUE;
}

gboolean
nautilus_preferences_is_initialized (void)
{
	return ((GLOBAL.preference_table != NULL) && (GLOBAL.gconf_client != NULL));
}

gboolean
nautilus_preferences_shutdown (void)
{
	g_assert (nautilus_preferences_is_initialized ());

	if (GLOBAL.preference_table != NULL) {
		g_hash_table_foreach (GLOBAL.preference_table,
				      preferences_hash_node_free_func,
				      NULL);
		
		g_hash_table_destroy (GLOBAL.preference_table);
		
		GLOBAL.preference_table = NULL;
	}
	
	gtk_object_unref (GTK_OBJECT (GLOBAL.gconf_client));

	GLOBAL.gconf_client = NULL;

	return TRUE;
}

