/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-prefs.h - Preference peek/poke/notify object implementation.

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
#include "nautilus-preferences.h"

#include <libgnome/gnome-config.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>

static const char PREFERENCES_GLOBAL_DOMAIN[] = "Nautilus::Global";

/*
 * PrefHashNode:
 *
 * A structure to manage preference hash table nodes.
 * Preferences are hash tables.  The hash key is the preference name
 * (a string).  The  hash value is a pointer of the following struct:
 */
typedef struct {
	NautilusPreferencesInfo	info;
	gpointer		value;
	GList			*callback_list;
} PrefHashNode;

/*
 * PrefCallbackInfo:
 *
 * A structure to manage callback lists.  A callback list is a GList.
 * The user_data in each list node is a pointer to the following 
 * struct:
 */
typedef struct {
	NautilusPreferencesCallback	callback_proc;
	gpointer			user_data;
	const PrefHashNode		*hash_node;
} PrefCallbackInfo;

/*
 * NautilusPreferencesDetails:
 *
 * Private memebers for NautilusPreferences.
 */
struct NautilusPreferencesDetails {
	char		*domain;
	GHashTable	*preference_hash_table;
};

/* NautilusPreferencesClass methods */
static void              nautilus_preferences_initialize_class        (NautilusPreferencesClass    *klass);
static void              nautilus_preferences_initialize              (NautilusPreferences         *preferences);

/* GtkObjectClass methods */
static void              nautilus_preferences_destroy                 (GtkObject                   *object);

/* PrefHashNode functions */
static PrefHashNode *    pref_hash_node_alloc                         (char                        *name,
								       char                        *description,
								       NautilusPreferencesType      type,
								       gconstpointer                default_value,
								       gpointer                     data);
static void              pref_hash_node_free                          (PrefHashNode                *pref_hash_node);
static void              pref_hash_node_free_func                     (gpointer                     key,
								       gpointer                     value,
								       gpointer                     user_data);

/* PrefCallbackInfo functions */
static PrefCallbackInfo *pref_callback_info_alloc                     (NautilusPreferencesCallback  callback_proc,
								       gpointer                     user_data,
								       const PrefHashNode          *hash_node);
static void              pref_callback_info_free                      (PrefCallbackInfo            *pref_hash_node);
static void              pref_callback_info_free_func                 (gpointer                     data,
								       gpointer                     user_data);
static void              pref_callback_info_invoke_func               (gpointer                     data,
								       gpointer                     user_data);
static void              pref_hash_node_add_callback                  (PrefHashNode                *pref_hash_node,
								       NautilusPreferencesCallback  callback_proc,
								       gpointer                     user_data);
static void              pref_hash_node_remove_callback               (PrefHashNode                *pref_hash_node,
								       NautilusPreferencesCallback  callback_proc,
								       gpointer                     user_data);
/* Private stuff */
static void              preference_set                               (NautilusPreferences         *preferences,
								       const char                  *name,
								       NautilusPreferencesType      type,
								       gconstpointer                value);
static void              preference_get                               (NautilusPreferences         *preferences,
								       const char                  *name,
								       NautilusPreferencesType      type,
								       gconstpointer               *value_out);
PrefHashNode *           prefs_hash_lookup                            (NautilusPreferences         *preferences,
								       const char                  *name);
PrefHashNode *           prefs_hash_lookup_with_implicit_registration (NautilusPreferences         *preferences,
								       const char                  *pref_name,
								       NautilusPreferencesType      pref_type);
static char *            gnome_config_make_string                     (char                        *name,
								       NautilusPreferencesType      type,
								       gconstpointer                default_value);
static void              preferences_register                         (NautilusPreferences         *preferences,
								       char                        *name,
								       char                        *description,
								       NautilusPreferencesType      type,
								       gconstpointer                default_value,
								       gpointer                     data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusPreferences, nautilus_preferences, GTK_TYPE_OBJECT)

/**
 * nautilus_preferences_initialize_class
 *
 * NautilusPreferencesClass class initialization method.
 * @preferences_class: The class to initialize.
 *
 **/
static void
nautilus_preferences_initialize_class (NautilusPreferencesClass *preferences_class)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (preferences_class);

 	parent_class = gtk_type_class (gtk_object_get_type ());

	/* GtkObjectClass */
	object_class->destroy = nautilus_preferences_destroy;
}

/**
 * nautilus_preferences_initialize
 *
 * GtkObject initialization method.
 * @object: The NautilusPreferences object to initialize.
 *
 **/
static void
nautilus_preferences_initialize (NautilusPreferences *preferences)
{
	preferences->details = g_new (NautilusPreferencesDetails, 1);

	preferences->details->domain = NULL;

	preferences->details->preference_hash_table = g_hash_table_new (g_str_hash, 
									g_str_equal);
}

/**
 * nautilus_preferences_destroy
 *
 * GtkObject destruction method.  Chains to super class.
 * @object: The NautilusPreferences object to destroy.
 *
 **/
static void
nautilus_preferences_destroy (GtkObject *object)
{
	NautilusPreferences *preferences;
	
	preferences = NAUTILUS_PREFERENCES (object);

	g_free (preferences->details->domain);
	
	if (preferences->details->preference_hash_table != NULL) {
		g_hash_table_foreach (preferences->details->preference_hash_table,
				      pref_hash_node_free_func,
				      NULL);
		
		g_hash_table_destroy (preferences->details->preference_hash_table);
		
		preferences->details->preference_hash_table = NULL;
	}
	
	g_free (preferences->details);
	
	/* Chain */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/**
 * pref_hash_node_alloc
 *
 * Allocate a preference hash node.
 * @info: Pointer to info structure to use for the node memebers.
 *
 * Return value: A newly allocated node.
 **/
static PrefHashNode *
pref_hash_node_alloc (char			*name,
		      char			*description,
		      NautilusPreferencesType	type,
		      gconstpointer		default_value,
		      gpointer			data)
{
	PrefHashNode * pref_hash_node;
	
	g_assert (name != NULL);

	pref_hash_node = g_new (PrefHashNode, 1);

	pref_hash_node->info.name = g_strdup (name);
	pref_hash_node->info.description = description ? g_strdup (description) : NULL;
	pref_hash_node->info.type = type;
	pref_hash_node->info.default_value = default_value;
	pref_hash_node->info.data = data;

	pref_hash_node->value = (gpointer) default_value;
	pref_hash_node->callback_list = NULL;

	return pref_hash_node;
}

/**
 * pref_hash_node_free
 *
 * Free a preference hash node members along with the node itself.
 * @pref_hash_node: The node to free.
 **/
static void
pref_hash_node_free (PrefHashNode *pref_hash_node)
{
	g_assert (pref_hash_node != NULL);

	g_assert (pref_hash_node->info.name != NULL);
	g_assert (pref_hash_node->info.description != NULL);

	g_list_foreach (pref_hash_node->callback_list,
			pref_callback_info_free_func,
			NULL);
	
	g_free (pref_hash_node->info.name);

	if (pref_hash_node->info.description)
		g_free (pref_hash_node->info.description);

	pref_hash_node->info.name = NULL;
	pref_hash_node->info.type = GTK_TYPE_INVALID;
	pref_hash_node->info.default_value = NULL;
	pref_hash_node->info.data = NULL;

	pref_hash_node->callback_list = NULL;
	pref_hash_node->value = NULL;

	g_free (pref_hash_node);
}

/**
 * pref_hash_node_add_callback
 *
 * Add a callback to a pref node.  Callbacks are fired whenever
 * the pref value changes.
 * @pref_hash_node: The hash node.
 * @callback_proc: The user supplied callback.
 * @user_data: The user supplied closure.
 **/
static void
pref_hash_node_add_callback (PrefHashNode			*pref_hash_node,
			     NautilusPreferencesCallback	callback_proc,
			     gpointer				user_data)
{
	PrefCallbackInfo * pref_callback_info;

	g_assert (pref_hash_node != NULL);

	g_assert (callback_proc != NULL);

	pref_callback_info = pref_callback_info_alloc (callback_proc, 
						       user_data,
						       pref_hash_node);

	g_assert (pref_callback_info != NULL);
	
	pref_hash_node->callback_list = g_list_append (pref_hash_node->callback_list, 
						       (gpointer) pref_callback_info);
}

/**
 * pref_hash_node_remove_callback
 *
 * remove a callback from a pref node.  Both the callback and the user_data must
 * match in order for a callback to be removed from the node.
 * @pref_hash_node: The hash node.
 * @callback_proc: The user supplied callback.
 * @user_data: The user supplied closure.
 **/
static void
pref_hash_node_remove_callback (PrefHashNode			*pref_hash_node,
				NautilusPreferencesCallback	callback_proc,
				gpointer			user_data)
{
	GList		 *new_list;
	GList		 *iterator;

	g_assert (pref_hash_node != NULL);

	g_assert (callback_proc != NULL);

	g_assert (pref_hash_node->callback_list != NULL);

	new_list = g_list_copy (pref_hash_node->callback_list);

	for (iterator = new_list; iterator != NULL; iterator = iterator->next) {
		PrefCallbackInfo *callback_info = (PrefCallbackInfo *) iterator->data;

		g_assert (callback_info != NULL);

		if (callback_info->callback_proc == callback_proc &&
		    callback_info->user_data == user_data) {

			pref_hash_node->callback_list = 
				g_list_remove (pref_hash_node->callback_list, 
					       (gpointer) callback_info);
			
			pref_callback_info_free (callback_info);
		}
	}
}

/**
 * pref_hash_node_free_func
 *
 * A function that frees a pref hash node.  It is meant to be fed to 
 * g_hash_table_foreach ()
 * @key: The hash key privately maintained by the GHashTable.
 * @value: The hash value privately maintained by the GHashTable.
 * @user_data: The user_data privately maintained by the GHashTable.
 **/
static void
pref_hash_node_free_func (gpointer key,
			  gpointer value,
			  gpointer user_data)
{
	PrefHashNode *pref_hash_node;

	pref_hash_node = (PrefHashNode *) value;

	g_assert (pref_hash_node != NULL);

	pref_hash_node_free (pref_hash_node);
}

/**
 * pref_callback_info_alloc
 *
 * Allocate a callback info struct from the given values.  PrefCallbackInfo
 * structures are used as nodes for the callbac_list member of pref hash table
 * nodes.
 * @callback_proc: The callback.
 * @user_data: The user data.
 * @hash_node: The hash table node this callback belongs to.
 *
 * Return value: A newly allocated node.
 **/
static PrefCallbackInfo *
pref_callback_info_alloc (NautilusPreferencesCallback	callback_proc,
			  gpointer			user_data,
			  const PrefHashNode		*hash_node)
{
	PrefCallbackInfo * pref_callback_info;
	
	g_assert (callback_proc != NULL);

	pref_callback_info = g_new (PrefCallbackInfo, 1);

	pref_callback_info->callback_proc = callback_proc;
	pref_callback_info->user_data = user_data;
	pref_callback_info->hash_node = hash_node;

	return pref_callback_info;
}

/**
 * pref_callback_info_free
 *
 * Free a callback info struct.
 * @pref_callback_info: The struct to free.
 **/
static void
pref_callback_info_free (PrefCallbackInfo *pref_callback_info)
{
	g_assert (pref_callback_info != NULL);

	pref_callback_info->callback_proc = NULL;
	pref_callback_info->user_data = NULL;

	g_free (pref_callback_info);
}

/**
 * pref_callback_info_free_func
 *
 * A function that frees a callback info struct.  It is meant to be fed to 
 * g_list_foreach ()
 * @data: The list data privately maintained by the GList.
 * @user_data: The user_data privately maintained by the GList.
 **/
static void
pref_callback_info_free_func (gpointer	data,
			      gpointer	user_data)
{
	PrefCallbackInfo *pref_callback_info;

	pref_callback_info = (PrefCallbackInfo *) data;

	g_assert (pref_callback_info != NULL);

	pref_callback_info_free (pref_callback_info);
}

/**
 * pref_callback_info_invoke_func
 *
 * A function that invokes a callback from the given struct.  It is meant to be fed to 
 * g_list_foreach ()
 * @data: The list data privately maintained by the GList.
 * @user_data: The user_data privately maintained by the GList.
 **/
static void
pref_callback_info_invoke_func (gpointer	data,
				gpointer	user_data)
{
 	NautilusPreferences     *preferences;
	PrefCallbackInfo *pref_callback_info;

	pref_callback_info = (PrefCallbackInfo *) data;

	g_assert (pref_callback_info != NULL);

	g_assert (pref_callback_info->callback_proc != NULL);

 	preferences = (NautilusPreferences *) user_data;

 	(* pref_callback_info->callback_proc) (preferences,
					       pref_callback_info->hash_node->info.name,
					       pref_callback_info->hash_node->value,
					       pref_callback_info->user_data);
}

static void
preference_set (NautilusPreferences	*preferences,
		const char		*name,
		NautilusPreferencesType	type,
		gconstpointer		value)
{
	PrefHashNode * pref_hash_node;

	g_assert (NAUTILUS_IS_PREFERENCES (preferences));
	g_assert (name != NULL);

	pref_hash_node = prefs_hash_lookup_with_implicit_registration (preferences, name, type);

	g_assert (pref_hash_node != NULL);

	/* gnome-config for now ; in the future gconf */
	switch (pref_hash_node->info.type) {

	case NAUTILUS_PREFERENCE_BOOLEAN:
		pref_hash_node->value = (gpointer) value;
		gnome_config_set_bool (name, GPOINTER_TO_INT (value));
		break;

	case NAUTILUS_PREFERENCE_ENUM:
		pref_hash_node->value = (gpointer) value;
		gnome_config_set_int (name, GPOINTER_TO_INT (value));
		break;

	case NAUTILUS_PREFERENCE_STRING:
		if (pref_hash_node->value)
			g_free (pref_hash_node->value);
		pref_hash_node->value = g_strdup (value);
		gnome_config_set_string (name, pref_hash_node->value);
		break;
	}

	/* Sync all the damn time.  Yes it sucks.  it will be better with gconf */
	gnome_config_sync ();

	/* Invoke callbacks for this node */
	if (pref_hash_node->callback_list) {
		g_list_foreach (pref_hash_node->callback_list,
				pref_callback_info_invoke_func,
				(gpointer) preferences);
	}
}

static void
preference_get (NautilusPreferences	*preferences,
		const char		*name,
		NautilusPreferencesType	type,
		gconstpointer		*value_out)
{
	PrefHashNode *pref_hash_node;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES (preferences));
	g_return_if_fail (name != NULL);
	g_return_if_fail (value_out != NULL);

	pref_hash_node = prefs_hash_lookup_with_implicit_registration (preferences, name, type);

	g_assert (pref_hash_node != NULL);

	*value_out = pref_hash_node->value;
}

/*
 * NautilusPreferences public methods
 */
GtkObject *
nautilus_preferences_new (const char *domain)
{
	NautilusPreferences *preferences;

	g_return_val_if_fail (domain != NULL, NULL);

	preferences = gtk_type_new (nautilus_preferences_get_type ());

	return GTK_OBJECT (preferences);
}

static void
preferences_register (NautilusPreferences	*preferences,
		      char			*name,
		      char			*description,
		      NautilusPreferencesType	type,
		      gconstpointer		default_value,
		      gpointer			data)
{
	char		*gnome_config_string;
	PrefHashNode	*pref_hash_node;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES (preferences));

	g_return_if_fail (name != NULL);
	g_return_if_fail (description != NULL);
	
	pref_hash_node = prefs_hash_lookup (preferences, name);

	if (pref_hash_node) {
		g_warning ("the '%s' preference is already registered", name);
		return;
	}

	pref_hash_node = pref_hash_node_alloc (name, description, type, default_value, data);

	g_hash_table_insert (preferences->details->preference_hash_table,
			     (gpointer) name,
			     (gpointer) pref_hash_node);

	gnome_config_string = gnome_config_make_string (name, type, default_value);

	g_assert (gnome_config_string != NULL);

	/* gnome-config for now; in the future gconf */
	switch (pref_hash_node->info.type) {
	case NAUTILUS_PREFERENCE_BOOLEAN:
		pref_hash_node->value = GINT_TO_POINTER (gnome_config_get_bool (gnome_config_string));
		break;

	case NAUTILUS_PREFERENCE_ENUM:
		pref_hash_node->value = GINT_TO_POINTER (gnome_config_get_int (gnome_config_string));
		break;

	case NAUTILUS_PREFERENCE_STRING:
		pref_hash_node->value = gnome_config_get_string (gnome_config_string);
		break;
	}

	g_free (gnome_config_string);

	/* Sync all the damn time.  Yes it sucks.  it will be better with gconf */
	gnome_config_sync ();
}

/**
 * gnome_config_make_string
 *
 * Make a gnome_config conformant string out of NautilusPreferencesInfo.  The 'path'
 * for the config string is the same for both gnome_config and nautilus preferences.
 * The only difference is that gnome_config strings can optionally have a "=default" 
 * appended to them to specify a default value for a preference.  In nautilus we 
 * separate the default value into a info structure member and thus the need to
 * for this function. 
 * @info: Pointer to info structure to use for the node memebers.
 *
 * Return value: A newly allocated string with the gnome_config conformant string.
 **/
static char *
gnome_config_make_string (char				*name,
			  NautilusPreferencesType	type,
			  gconstpointer			default_value)
{
	char * rv = NULL;
	GString * tmp = NULL;

	g_assert (name != NULL);

	tmp = g_string_new (name);
	
	switch (type) {
	case NAUTILUS_PREFERENCE_BOOLEAN:
		g_string_append (tmp, "=");

		if (GPOINTER_TO_INT (default_value)) {
			g_string_append (tmp, "true");
		} else {
			g_string_append (tmp, "false");
		}
		break;

	case NAUTILUS_PREFERENCE_ENUM:
		g_string_append (tmp, "=");

		g_string_sprintfa  (tmp, "%d", GPOINTER_TO_INT (default_value));
		break;
	
	case NAUTILUS_PREFERENCE_STRING:
		if (default_value != NULL)
		{
			g_string_append (tmp, "=");
			g_string_append  (tmp, (char *) default_value);
		}
		break;
	}
	
	g_assert (tmp != NULL);
	g_assert (tmp->str != NULL);

	rv = g_strdup (tmp->str);

	g_string_free (tmp, TRUE);

	return rv;
}

const NautilusPreferencesInfo *
nautilus_preferences_get_info (NautilusPreferences   *preferences,
			       const char            *name)
{
	PrefHashNode * pref_hash_node;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES (preferences), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	pref_hash_node = prefs_hash_lookup (preferences, name);
	
	g_assert (pref_hash_node != NULL);

	return &pref_hash_node->info;
}

void
nautilus_preferences_set_info (NautilusPreferences	*preferences,
			       const char		*name,
			       const char		*description,
			       NautilusPreferencesType	type,
			       gconstpointer		default_value,
			       gpointer			data)
{
	PrefHashNode *pref_hash_node;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES (preferences));
	g_return_if_fail (name != NULL);

	pref_hash_node = prefs_hash_lookup_with_implicit_registration (preferences, name, type);
	
	g_assert (pref_hash_node != NULL);

	pref_hash_node->info.default_value = default_value;

	if (pref_hash_node->info.description)
		g_free (pref_hash_node->info.description);

	pref_hash_node->info.description = g_strdup (description);

	pref_hash_node->info.data = data;

	if (!pref_hash_node->value)
		preference_set (preferences,
				name,
				type,
				default_value);
}

PrefHashNode *
prefs_hash_lookup (NautilusPreferences	*preferences,
		   const char           *name)
{
	gpointer hash_value;

	g_assert (preferences != NULL);
	g_assert (name != NULL);

	hash_value = g_hash_table_lookup (preferences->details->preference_hash_table,
					  (gconstpointer) name);

	return (PrefHashNode *) hash_value;
}

PrefHashNode *
prefs_hash_lookup_with_implicit_registration (NautilusPreferences	*preferences,
					      const char		*name,
					      NautilusPreferencesType   type)
{
	PrefHashNode * hash_node;

	g_assert (preferences != NULL);
	g_assert (name != NULL);

	hash_node = prefs_hash_lookup (preferences, name);

	if (!hash_node) {
		preferences_register (preferences,
				      (char *) name,
				      "Unspecified Description",
				      type,						   
				      (gconstpointer) 0,					   
				      (gpointer) NULL);
		
		hash_node = prefs_hash_lookup (preferences, name);
	}

	g_assert (hash_node != NULL);

	return hash_node;
}

gboolean
nautilus_preferences_add_boolean_callback (NautilusPreferences		*preferences,
					   const char			*name,
					   NautilusPreferencesCallback	callback_proc,
					   gpointer			user_data)
{
	PrefHashNode *pref_hash_node;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES (preferences), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (callback_proc != NULL, FALSE);

	pref_hash_node = prefs_hash_lookup_with_implicit_registration (preferences, name, NAUTILUS_PREFERENCE_BOOLEAN);

	if (pref_hash_node == NULL) {
		g_warning ("trying to add a callback for an unregistered preference");
		return FALSE;
	}

	pref_hash_node_add_callback (pref_hash_node,
				     callback_proc,
				     user_data);

	return TRUE;
}					

gboolean
nautilus_preferences_add_enum_callback (NautilusPreferences		*preferences,
					const char			*name,
					NautilusPreferencesCallback	callback_proc,
					gpointer			user_data)
{
	PrefHashNode *pref_hash_node;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES (preferences), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (callback_proc != NULL, FALSE);

	pref_hash_node = prefs_hash_lookup_with_implicit_registration (preferences, name, NAUTILUS_PREFERENCE_ENUM);

	if (pref_hash_node == NULL) {
		g_warning ("trying to add a callback for an unregistered preference");
		return FALSE;
	}

	pref_hash_node_add_callback (pref_hash_node,
				     callback_proc,
				     user_data);

	return TRUE;
}

gboolean
nautilus_preferences_add_string_callback (NautilusPreferences		*preferences,
					  const char			*name,
					  NautilusPreferencesCallback	callback_proc,
					  gpointer			user_data)
{
	PrefHashNode *pref_hash_node;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES (preferences), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (callback_proc != NULL, FALSE);

	pref_hash_node = prefs_hash_lookup_with_implicit_registration (preferences, name, NAUTILUS_PREFERENCE_STRING);

	if (pref_hash_node == NULL) {
		g_warning ("trying to add a callback for an unregistered preference");
		return FALSE;
	}

	pref_hash_node_add_callback (pref_hash_node,
				     callback_proc,
				     user_data);

	return TRUE;
}

gboolean
nautilus_preferences_remove_callback (NautilusPreferences	   *preferences,
				      const char		   *name,
				      NautilusPreferencesCallback  callback_proc,
				      gpointer			   user_data)
{
	PrefHashNode *pref_hash_node;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES (preferences), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (callback_proc != NULL, FALSE);

	pref_hash_node = prefs_hash_lookup (preferences, name);
	if (pref_hash_node == NULL) {
		g_warning ("trying to remove a callback for an unregistered preference");
		return FALSE;
	}

	pref_hash_node_remove_callback (pref_hash_node,
					callback_proc,
					user_data);

	return TRUE;
}

void
nautilus_preferences_set_boolean (NautilusPreferences     *preferences,
				  const char		  *name,
				  gboolean		  boolean_value)
{
	g_return_if_fail (NAUTILUS_IS_PREFERENCES (preferences));
	g_return_if_fail (name != NULL);

	preference_set (preferences, 
			name,
			NAUTILUS_PREFERENCE_BOOLEAN,
			GINT_TO_POINTER (boolean_value));
}

gboolean
nautilus_preferences_get_boolean (NautilusPreferences  *preferences,
				  const char    *name)
{
	gconstpointer value;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES (preferences), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	preference_get (preferences,
			name,
			NAUTILUS_PREFERENCE_BOOLEAN,
			&value);

	return GPOINTER_TO_INT (value);
}

void
nautilus_preferences_set_enum (NautilusPreferences	*preferences,
			       const char    *name,
			       int           enum_value)
{
	g_return_if_fail (NAUTILUS_IS_PREFERENCES (preferences));
	g_return_if_fail (name != NULL);

	preference_set (preferences,
			name,
			NAUTILUS_PREFERENCE_ENUM,
			GINT_TO_POINTER (enum_value));
}

int
nautilus_preferences_get_enum (NautilusPreferences  *preferences,
			       const char    *name)
{
	gconstpointer value;
	
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES (preferences), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	
	preference_get (preferences,
			name,
			NAUTILUS_PREFERENCE_ENUM,
			&value);
	
	return GPOINTER_TO_INT (value);
}

void
nautilus_preferences_set_string (NautilusPreferences *preferences,
				 const char *name,
				 const char *value)
{
	g_return_if_fail (NAUTILUS_IS_PREFERENCES (preferences));
	g_return_if_fail (name != NULL);
	g_return_if_fail (value != NULL);

	preference_set (preferences,
			name,
			NAUTILUS_PREFERENCE_STRING,
			value);
}

char *
nautilus_preferences_get_string (NautilusPreferences *preferences,
				 const char *name)
{
	gconstpointer value;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES (preferences), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	preference_get (preferences,
			name,
			NAUTILUS_PREFERENCE_STRING,
			&value);
	
	return g_strdup (value);
}

NautilusPreferences *
nautilus_preferences_get_global_preferences (void)
{
	static GtkObject * global_preferences = NULL;

	if (global_preferences == NULL) {
		global_preferences = nautilus_preferences_new (PREFERENCES_GLOBAL_DOMAIN);
	}

	return NAUTILUS_PREFERENCES (global_preferences);
}
