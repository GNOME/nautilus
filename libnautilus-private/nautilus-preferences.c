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
#include "nautilus-preferences-private.h"
#include "nautilus-user-level-manager.h"

#include "nautilus-gtk-macros.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-string.h"

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <gtk/gtksignal.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
 
static const char PREFERENCES_GCONF_PATH[] = "/apps/nautilus";

#include <gtk/gtksignal.h>

/*
 * PreferencesHashNode:
 *
 * A structure to manage preference hash table nodes.
 * Preferences are hash tables.  The hash key is the preference name
 * (a string).  The  hash value is a pointer of the following struct:
 */
typedef struct {
	GList	*callback_list;
	int	gconf_connections[3];
	char	*name;
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
	guint		  old_user_level;

} PreferencesGlobalData;

static PreferencesGlobalData GLOBAL = { NULL, NULL, 0 };

/* PreferencesHashNode functions */
static PreferencesHashNode *    preferences_hash_node_alloc                       (const char                  *name);
static void                     preferences_hash_node_free                        (PreferencesHashNode         *node);
static void                     preferences_hash_node_free_func                   (gpointer                     key,
										   gpointer                     value,
										   gpointer                     callback_data);
static void                     preferences_hash_node_check_changes_func          (gpointer                     key,
										   gpointer                     value,
										   gpointer                     callback_data);

/* PreferencesCallbackNode functions */
static PreferencesCallbackNode *preferences_callback_node_alloc                   (NautilusPreferencesCallback  callback_proc,
										   gpointer                     callback_data,
										   const PreferencesHashNode   *hash_node);
static void                     preferences_callback_node_free                    (PreferencesCallbackNode     *node);
static void                     preferences_callback_node_free_func               (gpointer                     data,
										   gpointer                     callback_data);
static void                     preferences_callback_node_invoke_func             (gpointer                     data,
										   gpointer                     callback_data);
static void                     preferences_hash_node_add_by_user_level_callbacks (PreferencesHashNode         *node,
										   NautilusPreferencesCallback  callback_proc,
										   gpointer                     callback_data);
static void                     preferences_hash_node_add_callback                (PreferencesHashNode         *node,
										   NautilusPreferencesCallback  callback_proc,
										   gpointer                     callback_data);
static void                     preferences_hash_node_remove_callback             (PreferencesHashNode         *node,
										   NautilusPreferencesCallback  callback_proc,
										   gpointer                     callback_data);

/* Private stuff */
static PreferencesHashNode *    preferences_hash_node_lookup                      (const char                  *name);
static void                     preferences_register                              (const char                  *name);
static gboolean                 preferences_initialize_if_needed                  (void);
static char *                   preferences_make_make_gconf_key                   (const char                  *preference_name);

/* GConf callbacks */
static void                     preferences_gconf_by_user_level_callback          (GConfClient                 *client,
										   guint                        cnxn_id,
										   const gchar                 *key,
										   GConfValue                  *value,
										   gboolean                     is_default,
										   gpointer                     user_data);
static void                     preferences_gconf_callback          (GConfClient                 *client,
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
preferences_hash_node_alloc (const char			*name)
{
	PreferencesHashNode * node;
	
	g_assert (name != NULL);

	node = g_new (PreferencesHashNode, 1);

	node->name = g_strdup(name);

	node->callback_list = NULL;

	node->gconf_connections[0] = 0;
	node->gconf_connections[1] = 0;
	node->gconf_connections[2] = 0;

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
	guint i;

	g_assert (node != NULL);

	/* Remove the gconf notification if its still lingering */
	for (i = 0; i < 3; i++) 
	{
		if (node->gconf_connections[i] != 0)
		{
			gconf_client_notify_remove (GLOBAL.gconf_client,
						    node->gconf_connections[i]);
			
			node->gconf_connections[i] = 0;
		}
	}

	nautilus_g_list_free_deep_custom (node->callback_list,
					  preferences_callback_node_free_func,
					  NULL);
	
	node->callback_list = NULL;

	g_free (node->name);

	g_free (node);
}

/**
 * preferences_hash_node_add_by_user_level_callbacks
 *
 * Add a callback to a pref node.  Callbacks are fired whenever
 * the pref value changes.
 * @preferences_hash_node: The hash node.
 * @callback_proc: The user supplied callback.
 * @callback_data: The user supplied closure.
 **/
static void
preferences_hash_node_add_by_user_level_callbacks (PreferencesHashNode		*node,
						   NautilusPreferencesCallback	callback_proc,
						   gpointer			callback_data)
{
	PreferencesCallbackNode	*preferences_callback_node;
	guint i;

	g_assert (node != NULL);

	g_assert (callback_proc != NULL);

	preferences_callback_node = preferences_callback_node_alloc (callback_proc, 
								     callback_data,
								     node);
	
	g_assert (preferences_callback_node != NULL);
	
	node->callback_list = g_list_append (node->callback_list, 
					     (gpointer) preferences_callback_node);

	/*
	 * We install only one gconf notification for each preference node.
	 * Otherwise, we would invoke the installed callbacks more than once
	 * per registered callback.
	 */
	for (i = 0; i < 3; i++) 
	{
		if (node->gconf_connections[i] == 0) {
			char *key;
			GConfError *error = NULL;

			g_assert (node->name != NULL);
			g_assert (node->gconf_connections[i] == 0);
			
			key = nautilus_user_level_manager_make_gconf_key (node->name, i);
			g_assert (key);

			node->gconf_connections[i] = gconf_client_notify_add (GLOBAL.gconf_client,
									      key,
									      preferences_gconf_by_user_level_callback,
									      node,
									      NULL,
									      &error);
			if (nautilus_preferences_handle_error (&error)) {
				node->gconf_connections[i] = 0;
			}
			
			g_free (key);
		}
	}
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
	PreferencesCallbackNode	*preferences_callback_node;

	g_assert (node != NULL);
	g_assert (callback_proc != NULL);

	preferences_callback_node = preferences_callback_node_alloc (callback_proc, 
								     callback_data,
								     node);
	
	g_assert (preferences_callback_node != NULL);
	
	node->callback_list = g_list_append (node->callback_list, 
					     (gpointer) preferences_callback_node);

	/*
	 * We install only one gconf notification for each preference node.
	 * Otherwise, we would invoke the installed callbacks more than once
	 * per registered callback.
	 */
	if (node->gconf_connections[0] == 0) {
		GConfError *error = NULL;

		g_assert (node->name != NULL);
		g_assert (node->gconf_connections[0] == 0);
		
		node->gconf_connections[0] = gconf_client_notify_add (GLOBAL.gconf_client,
								      node->name,
								      preferences_gconf_callback,
								      node,
								      NULL,
								      &error);
		if (nautilus_preferences_handle_error (&error)) {
			node->gconf_connections[0] = 0;
		}
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
			
			preferences_callback_node_free (callback_info);
		}
	}
	
	/*
	 * If there are no callbacks left in the node, remove the gconf 
	 * notification as well.
	 */
	if (node->callback_list == NULL) {
		guint i;

		for (i = 0; i < 3; i++) 
		{
			/* we don't assert here. if we had trouble with gconf
			 * notify addition, this would be 0 */
			if (node->gconf_connections[i] != 0) {
				gconf_client_notify_remove (GLOBAL.gconf_client,
							    node->gconf_connections[i]);
			}
			
			node->gconf_connections[i] = 0;
		}
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

static void
preferences_hash_node_check_changes_func (gpointer key,
					  gpointer value,
					  gpointer user_data)
{
	PreferencesHashNode	*node;
	guint			old_user_level;
	guint			new_user_level;

	g_assert (value != NULL);
	
	node = (PreferencesHashNode *) value;

	/* Ignore preferences that are not coupled to user level */
	if (nautilus_str_has_prefix (node->name, "/")) {
		return;
	}
	
	old_user_level = GPOINTER_TO_UINT (user_data);
	new_user_level = nautilus_user_level_manager_get_user_level ();

	/* FIXME bugzilla.eazel.com 1273: 
	 * This currently only works for keys, it doesnt work with whole namespaces 
	 */
	if (!nautilus_user_level_manager_compare_preference_between_user_levels (node->name,
										 old_user_level,
										 new_user_level)) {
		/* Invoke callbacks for this node */
		if (node->callback_list) {
			g_list_foreach (node->callback_list,
					preferences_callback_node_invoke_func,
					(gpointer) NULL);
		}
	}
}

/**
 * preferences_callback_node_alloc
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
preferences_callback_node_alloc (NautilusPreferencesCallback	callback_proc,
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
 * preferences_callback_node_free
 *
 * Free a callback info struct.
 * @preferences_callback_node: The struct to free.
 **/
static void
preferences_callback_node_free (PreferencesCallbackNode *callback_node)
{
	g_assert (callback_node != NULL);

	callback_node->callback_proc = NULL;
	callback_node->callback_data = NULL;

	g_free (callback_node);
}

/**
 * preferences_callback_node_free_func
 *
 * A function that frees a callback info struct.  It is meant to be fed to 
 * g_list_foreach ()
 * @data: The list data privately maintained by the GList.
 * @callback_data: The callback_data privately maintained by the GList.
 **/
static void
preferences_callback_node_free_func (gpointer	data,
				     gpointer	callback_data)
{
	g_assert (data != NULL);

	preferences_callback_node_free ((PreferencesCallbackNode *) data);
}

/**
 * preferences_callback_node_invoke_func
 *
 * A function that invokes a callback from the given struct.  It is meant to be fed to 
 * g_list_foreach ()
 * @data: The list data privately maintained by the GList.
 * @callback_data: The callback_data privately maintained by the GList.
 **/
static void
preferences_callback_node_invoke_func (gpointer	data,
				       gpointer	callback_data)
{
	PreferencesCallbackNode	*callback_node;

	callback_node = (PreferencesCallbackNode *) data;

	g_assert (callback_node != NULL);
	g_assert (callback_node->callback_proc != NULL);

 	(* callback_node->callback_proc) (callback_node->callback_data);
}

static char *
preferences_make_make_gconf_key (const char *preference_name)
{
	if (nautilus_str_has_prefix (preference_name, "/")) {
		//g_print ("key for %s is %s\n", preference_name, preference_name);
		return g_strdup (preference_name);
	}

#if 0
	{
		char *foo = nautilus_user_level_manager_make_current_gconf_key (preference_name);
		g_print ("key for %s is %s\n", preference_name, foo);
		g_free (foo);
	}
#endif

	return nautilus_user_level_manager_make_current_gconf_key (preference_name);
}

static void
preferences_register (const char			*name)
{
	PreferencesHashNode *node;

	g_return_if_fail (name != NULL);

	preferences_initialize_if_needed ();
	
	node = preferences_hash_node_lookup (name);

	if (node) {
		g_warning ("the '%s' preference is already registered", name);
		return;
	}

	node = preferences_hash_node_alloc (name);

	g_hash_table_insert (GLOBAL.preference_table, (gpointer) name, (gpointer) node);
}

static PreferencesHashNode *
preferences_hash_node_lookup (const char *name)
{
	gpointer hash_value;

	g_assert (name != NULL);

	preferences_initialize_if_needed ();

	hash_value = g_hash_table_lookup (GLOBAL.preference_table, (gconstpointer) name);
	
	return (PreferencesHashNode *) hash_value;
}

static void
preferences_gconf_by_user_level_callback (GConfClient	*client, 
					  guint		connection_id, 
					  const gchar	*key, 
					  GConfValue	*value, 
					  gboolean	is_default, 
					  gpointer	user_data)
{
	PreferencesHashNode	*node;
	const char		*expected_name;
	char			*expected_key;
	GConfError		*error = NULL;

	g_return_if_fail (user_data != NULL);

	node = (PreferencesHashNode *) user_data;

	expected_name = node->name;
	g_assert (expected_name != NULL);

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
	expected_key = nautilus_user_level_manager_make_current_gconf_key (expected_name);

	if (strcmp (key, expected_key) != 0) {

		/* The prefix should be the same */
		if (strncmp (key, expected_key, strlen (expected_key)) != 0) {

			/* FIXME bugzilla.eazel.com 1272: This is triggering the first time the beast runs
			 * without an existing ~/.gconf directory.
			 */
#if 0
 			g_warning ("preferences_gconf_by_user_level_callback: Wrong prefix!  This indicates a bug.\n");
#endif
			g_free (expected_key);
			return;
		}

		key = expected_key;
	}

	g_assert (key != NULL);
	
	node = preferences_hash_node_lookup (expected_name);
	
	g_assert (node != NULL);

	gconf_client_suggest_sync (GLOBAL.gconf_client, &error);
	nautilus_preferences_handle_error (&error);

	/* Invoke callbacks for this node */
	if (node->callback_list) {
		g_list_foreach (node->callback_list,
				preferences_callback_node_invoke_func,
				(gpointer) NULL);
	}

	g_free (expected_key);
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
	GConfError		*error = NULL;

	g_return_if_fail (user_data != NULL);
	
	node = (PreferencesHashNode *) user_data;
	g_assert (node != NULL);
	
	g_assert (nautilus_str_is_equal ( node->name, key));

	gconf_client_suggest_sync (GLOBAL.gconf_client, &error);
	nautilus_preferences_handle_error (&error);

	/* Invoke callbacks for this node */
	if (node->callback_list) {
		g_list_foreach (node->callback_list,
				preferences_callback_node_invoke_func,
				(gpointer) NULL);
	}
}

static void
user_level_changed_callback (GtkObject	*user_level_manager,
			     gpointer	user_data)
{
	guint new_user_level;

	new_user_level = nautilus_user_level_manager_get_user_level ();

	g_hash_table_foreach (GLOBAL.preference_table,
			      preferences_hash_node_check_changes_func,
			      GUINT_TO_POINTER (GLOBAL.old_user_level));

	GLOBAL.old_user_level = new_user_level;
}

static gboolean
preferences_initialize_if_needed (void)
{
	GConfError	  *error = NULL;

	if (GLOBAL.preference_table != NULL && GLOBAL.gconf_client != NULL) {
		return TRUE;
	}
	
	if (!gconf_is_initialized ()) {
		char		  *argv[] = { "nautilus", NULL };
		
		if (!gconf_init (1, argv, &error)) {
			g_assert (error != NULL);

			nautilus_preferences_handle_error (&error);
			
			return FALSE;
		}
	}
	
	g_assert (GLOBAL.preference_table == NULL);
	g_assert (GLOBAL.gconf_client == NULL);

	GLOBAL.preference_table = g_hash_table_new (g_str_hash, g_str_equal);

	g_assert (GLOBAL.preference_table != NULL);

 	GLOBAL.gconf_client = gconf_client_get_default ();

	g_assert (GLOBAL.gconf_client != NULL);

	/* Let gconf know about ~/.gconf/nautilus */
	gconf_client_add_dir (GLOBAL.gconf_client,
			      PREFERENCES_GCONF_PATH,
			      GCONF_CLIENT_PRELOAD_RECURSIVE,
			      &error);
	nautilus_preferences_handle_error (&error);

	GLOBAL.old_user_level = nautilus_user_level_manager_get_user_level ();
	
	/* Register to find out about user level changes */
	gtk_signal_connect (GTK_OBJECT (nautilus_user_level_manager_get ()),
			    "user_level_changed",
			    user_level_changed_callback,
			    NULL);

	return TRUE;
}


/*
 * Public functions
 */
gboolean
nautilus_preferences_add_callback (const char			*name,
				   NautilusPreferencesCallback	callback_proc,
				   gpointer			callback_data)
{
	PreferencesHashNode *node;

	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (callback_proc != NULL, FALSE);

	preferences_initialize_if_needed ();

	node = preferences_hash_node_lookup (name);

	if (!node) {
		preferences_register (name);
		
		node = preferences_hash_node_lookup (name);
	}

	g_assert (node != NULL);

	if (node == NULL) {
		g_warning ("trying to add a callback for an unregistered preference");
		return FALSE;
	}

	if (nautilus_str_has_prefix (name, "/")) {
		preferences_hash_node_add_callback (node, callback_proc, callback_data);
	}
	else {
		preferences_hash_node_add_by_user_level_callbacks (node, callback_proc, callback_data);
	}

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
	GConfError      *error = NULL;
	char		*key;
	gboolean	 old_value;

	g_return_if_fail (name != NULL);

	preferences_initialize_if_needed ();
	
	key = preferences_make_make_gconf_key (name);
	g_assert (key != NULL);

	/* Make sure the preference value is indeed different */
	old_value = gconf_client_get_bool (GLOBAL.gconf_client, key, &error);
	if (error == NULL
	    && old_value == boolean_value) {
		g_free (key);
		return;
	}
	nautilus_preferences_handle_error (&error);

	gconf_client_set_bool (GLOBAL.gconf_client, key, boolean_value, &error);
	nautilus_preferences_handle_error (&error);

	gconf_client_suggest_sync (GLOBAL.gconf_client, &error);
	nautilus_preferences_handle_error (&error);

	g_free (key);
}

gboolean
nautilus_preferences_get_boolean (const char	*name,
				  gboolean	default_value)
{
	GConfError      *error = NULL;
	gboolean         result;
	char		*key;

	g_return_val_if_fail (name != NULL, FALSE);

	preferences_initialize_if_needed ();

	key = preferences_make_make_gconf_key (name);
	g_assert (key != NULL);

	result = gconf_client_get_bool (GLOBAL.gconf_client, key, &error);
	if (nautilus_preferences_handle_error (&error)) {
		result = default_value;
	}

	g_free (key);

	return result;
}


void
nautilus_preferences_set_string_list (const char	*name,
				      GSList            *string_list_value)
{
	GConfError      *error = NULL;
	char		*key;

	g_return_if_fail (name != NULL);

	preferences_initialize_if_needed ();

	key = preferences_make_make_gconf_key (name);
	g_assert (key != NULL);

	/* FIXME bugzilla.eazel.com 2543: Make sure the preference value is indeed different
           before setting, like the other functions */

	gconf_client_set_list (GLOBAL.gconf_client, key,
			       GCONF_VALUE_STRING,
			       string_list_value,
			       &error);
	nautilus_preferences_handle_error (&error);

	gconf_client_suggest_sync (GLOBAL.gconf_client, &error);
	nautilus_preferences_handle_error (&error);

	g_free (key);
}

GSList *
nautilus_preferences_get_string_list (const char	*name)
{
	GConfError      *error = NULL;
	GSList          *result;
	char            *key;

	g_return_val_if_fail (name != NULL, FALSE);

	preferences_initialize_if_needed ();

	key = preferences_make_make_gconf_key (name);
	g_assert (key != NULL);

	result = gconf_client_get_list (GLOBAL.gconf_client, key, 
					GCONF_VALUE_STRING, &error);
	nautilus_preferences_handle_error (&error);

	g_free (key);

	return result;
}


void
nautilus_preferences_set_enum (const char    *name,
			       int           enum_value)
{
	GConfError      *error = NULL;
	int		 old_value;
	char		*key;

	g_return_if_fail (name != NULL);

	preferences_initialize_if_needed ();

	key = preferences_make_make_gconf_key (name);
	g_assert (key != NULL);

	/* Make sure the preference value is indeed different */
	old_value = gconf_client_get_int (GLOBAL.gconf_client, key, &error);
	if (error == NULL
	    && old_value == enum_value) {
		g_free (key);
		return;
	}
	nautilus_preferences_handle_error (&error);

	gconf_client_set_int (GLOBAL.gconf_client, key, enum_value, &error);
	nautilus_preferences_handle_error (&error);

	gconf_client_suggest_sync (GLOBAL.gconf_client, &error);
	nautilus_preferences_handle_error (&error);

	g_free (key);
}

int
nautilus_preferences_get_enum (const char	*name,
			       int		default_value)
{
	GConfError      *error = NULL;
	int              result;
	char		*key;

	g_return_val_if_fail (name != NULL, FALSE);

	preferences_initialize_if_needed ();

	key = preferences_make_make_gconf_key (name);
	g_assert (key != NULL);

	result = gconf_client_get_int (GLOBAL.gconf_client, key, &error);
	if (nautilus_preferences_handle_error (&error)) {
		result = default_value;
	}

	g_free (key);

	return result;
}

void
nautilus_preferences_set (const char *name, 
			  const char *value)
{
	GConfError      *error = NULL;
	char		*key;

	g_return_if_fail (name != NULL);

	preferences_initialize_if_needed ();

	key = preferences_make_make_gconf_key (name);
	g_assert (key != NULL);

	/* Make sure the preference value is indeed different */
	if (value) {
		char *current_value;
		int result;

		current_value = gconf_client_get_string (GLOBAL.gconf_client, key, &error);
		if (nautilus_preferences_handle_error (&error)) {
			result = ! 0;
		} else {
			result = nautilus_strcmp (current_value, value);
		}
		
		if (current_value) {
			g_free (current_value);
		}

		if (result == 0) {
			g_free (key);
			return;
		}
	}

	gconf_client_set_string (GLOBAL.gconf_client, key, value, &error);
	nautilus_preferences_handle_error (&error);

	gconf_client_suggest_sync (GLOBAL.gconf_client, &error);
	nautilus_preferences_handle_error (&error);

	g_free (key);
}

char *
nautilus_preferences_get (const char	*name,
			  const char	*default_value)
{
	GConfError	*error = NULL;
	gchar		*value = NULL;
	char		*key;

	g_return_val_if_fail (name != NULL, FALSE);

	preferences_initialize_if_needed ();

	key = preferences_make_make_gconf_key (name);
	g_assert (key != NULL);

	value = gconf_client_get_string (GLOBAL.gconf_client, key, &error);
	if (nautilus_preferences_handle_error (&error)) {
		/* note: g_free and g_strdup handle NULL correctly */
		g_free (value);
		value = g_strdup (default_value);
	}

	if (!value && default_value) {
		value = g_strdup (default_value);
	}

	g_free (key);
	
	return value;
}

void
nautilus_preferences_shutdown (void)
{
	if (GLOBAL.preference_table == NULL && GLOBAL.gconf_client == NULL) {
		return;
	}

	gtk_signal_disconnect_by_func (GTK_OBJECT (nautilus_user_level_manager_get ()),
				       user_level_changed_callback,
				       NULL);
	
	if (GLOBAL.preference_table != NULL) {
		g_hash_table_foreach (GLOBAL.preference_table,
				      preferences_hash_node_free_func,
				      NULL);
		
		g_hash_table_destroy (GLOBAL.preference_table);
		
		GLOBAL.preference_table = NULL;
	}

	if (GLOBAL.gconf_client != NULL) {
		gtk_object_unref (GTK_OBJECT (GLOBAL.gconf_client));

		GLOBAL.gconf_client = NULL;
	}

}

gboolean
nautilus_preferences_handle_error (GConfError **error)
{
	static gboolean shown_dialog = FALSE;

	g_return_val_if_fail (error != NULL, FALSE);

	if (*error != NULL) {
		g_warning (_("GConf error:\n  %s"), (*error)->str);
		if ( ! shown_dialog) {
			char *message;
			GtkWidget *dialog;

			shown_dialog = TRUE;

			message = g_strdup_printf (_("GConf error:\n  %s\n"
						     "All further errors shown "
						     "only on terminal"),
						   (*error)->str);

			dialog = gnome_error_dialog (message);
		}
		gconf_error_destroy(*error);
		*error = NULL;

		return TRUE;
	}

	return FALSE;
}
