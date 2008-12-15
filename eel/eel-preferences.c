/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-preferences.c - Preference peek/poke/notify implementation.

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
#include "eel-preferences.h"

#include "eel-debug.h"
#include "eel-gconf-extensions.h"
#include "eel-lib-self-check-functions.h"
#include "eel-enumeration.h"
#include "eel-glib-extensions.h"
#include "eel-string.h"
#include <gconf/gconf-client.h>
#include <gconf/gconf.h>
#include <gtk/gtk.h>

/* An enumeration used for updating auto-storage variables in a type-specific way. 
 * FIXME: there is another enumeration like this in eel-global-preferences.c,
 * used for different purposes but in a related way. Should we combine them?
 */
typedef enum {
	PREFERENCE_BOOLEAN = 1,
	PREFERENCE_INTEGER,
	PREFERENCE_STRING,
	PREFERENCE_STRING_ARRAY,
	PREFERENCE_STRING_ARRAY_AS_QUARKS
} PreferenceType;

/*
 * PreferencesEntry:
 *
 * A structure to manage preference hash table nodes.
 * Preferences are hash tables.  The hash key is the preference name
 * (a string).  The  hash value is a pointer of the following struct:
 */
typedef struct {
	char *name;
	char *description;
	PreferenceType type;
	gboolean invisible;
	GList *callback_list;
	GList *auto_storage_list;
	int gconf_connection_id;
	char *enumeration_id;
	GConfValue *fallback;
} PreferencesEntry;

/*
 * PreferencesCallbackEntry:
 *
 * A structure to manage callback lists.  A callback list is a GList.
 * The callback_data in each list node is a pointer to the following 
 * struct:
 */
typedef struct {
	EelPreferencesCallback callback;
	gpointer callback_data;
} PreferencesCallbackEntry;

static GHashTable *global_table = NULL;
static char *storage_path = NULL;
static gboolean initialized = FALSE;

static void              preferences_global_table_free             (void);
static char *            preferences_key_make                      (const char               *name);
static void              preferences_callback_entry_free           (PreferencesCallbackEntry *callback_entry);
static void              preferences_entry_update_auto_storage     (PreferencesEntry         *entry);
static PreferencesEntry *preferences_global_table_lookup_or_insert (const char               *name);

static int
preferences_gconf_value_get_int (const GConfValue *value)
{
	if (value == NULL) {
		return 0;
	}
	g_assert (value->type == GCONF_VALUE_INT);
	return gconf_value_get_int (value);
}

static gboolean
preferences_gconf_value_get_bool (const GConfValue *value)
{
	if (value == NULL) {
		return FALSE;
	}
	g_assert (value->type == GCONF_VALUE_BOOL);
	return gconf_value_get_bool (value);
}

static char *
preferences_gconf_value_get_string (const GConfValue *value)
{
	if (value == NULL) {
		return g_strdup ("");
	}
	g_assert (value->type == GCONF_VALUE_STRING);
	return g_strdup (gconf_value_get_string (value));
}

static char **
preferences_gconf_value_get_string_array (const GConfValue *value)
{
	GSList *slist, *l;
	GPtrArray *result;

	if (value == NULL) {
		return NULL;
	}

	g_assert (value->type == GCONF_VALUE_LIST);
	g_assert (gconf_value_get_list_type (value) == GCONF_VALUE_STRING);

	slist = eel_gconf_value_get_string_list (value);

	result = g_ptr_array_new ();
	for (l = slist; l != NULL; l = l->next) {
		g_ptr_array_add (result, l->data);
	}
	g_slist_free (slist);
	g_ptr_array_add (result, NULL);

 	return (char **) g_ptr_array_free (result, FALSE);
}

static const char *
preferences_peek_storage_path (void)
{
	g_assert (storage_path != NULL);

	return storage_path;
}

static void
preferences_set_storage_path (const char *new_storage_path)
{
	g_assert (eel_strlen (new_storage_path) > 0);

	/* Make sure the path is indeed different */
	if (eel_str_is_equal (new_storage_path, storage_path)) {
		return;
	}

	/* Free the preference hash table */
	preferences_global_table_free ();

	/* Stop monitoring the old path */
	eel_gconf_monitor_remove (storage_path);

	g_free (storage_path);
	storage_path = g_strdup (new_storage_path);

	/* Start monitoring the new path */
	eel_gconf_monitor_add (storage_path);
}

static gboolean
preferences_is_initialized (void)
{
	return initialized;
}

static GConfValue *
preferences_get_value (const char *name)
{
	GConfValue *result;
	char *key;
	PreferencesEntry *entry;
	
	g_assert (name != NULL);
	g_assert (preferences_is_initialized ());

	key = preferences_key_make (name);
	result = eel_gconf_get_value (key);
	g_free (key);

	if (result == NULL) {		
		entry = preferences_global_table_lookup_or_insert (name);

		if (entry->fallback)
			result = gconf_value_copy (entry->fallback);
	}
	
	return result;
}

/* If the preference name begind with a "/", we interpret 
 * it as a straight gconf key. */
static gboolean
preferences_preference_is_gconf_key (const char *name)
{
	g_assert (name != NULL);
	
	if (eel_str_has_prefix (name, "/")) {
		return FALSE;
	}
	
	return TRUE;
}

static char *
preferences_key_make (const char *name)
{
	g_assert (name != NULL);
	
	if (!preferences_preference_is_gconf_key (name)) {
		return g_strdup (name);
	}

	/* Otherwise, we prefix it with the path */
	return g_strconcat (preferences_peek_storage_path (), "/",
			    name, NULL);
}

/* Get default from schema or emergency fallback */
static GConfValue *
preferences_get_default_value (const char *name)
{
	GConfValue *result;
	PreferencesEntry *entry;
	char *key;

	g_assert (name != NULL);

	key = preferences_key_make (name);

	result = eel_gconf_get_default_value (key);

	g_free (key);

	if (result == NULL) {
		entry = preferences_global_table_lookup_or_insert (name);
		if (entry && entry->fallback)
			result = gconf_value_copy (entry->fallback);
	}
	
        return result;
}

static int
preferences_callback_entry_compare (gconstpointer a,
				    gconstpointer b)
{
	const PreferencesCallbackEntry *a_entry = a;
	const PreferencesCallbackEntry *b_entry = b;

	if (a_entry->callback < b_entry->callback) {
		return -1;
	}

	if (a_entry->callback > b_entry->callback) {
		return +1;
	}

	if (a_entry->callback_data < b_entry->callback_data) {
		return -1;
	}

	if (a_entry->callback_data > b_entry->callback_data) {
		return +1;
	}

	return 0;
}

/* Public preferences functions */

gboolean
eel_preferences_get_is_invisible (const char *name)
{
	g_assert (name != NULL);
	g_assert (preferences_is_initialized ());

	return preferences_global_table_lookup_or_insert (name)->invisible;
}

void
eel_preferences_set_is_invisible (const char *name,
				  gboolean is_invisible)
{
	g_return_if_fail (name != NULL);
	g_return_if_fail (preferences_is_initialized ());

	preferences_global_table_lookup_or_insert (name)->invisible = is_invisible;
}

void
eel_preferences_set_boolean (const char *name,
			     gboolean boolean_value)
{
	char *key;

	g_return_if_fail (name != NULL);
	g_return_if_fail (preferences_is_initialized ());
	
	key = preferences_key_make (name);
	eel_gconf_set_boolean (key, boolean_value);
	g_free (key);

	eel_gconf_suggest_sync ();
}

gboolean
eel_preferences_get_boolean (const char *name)
{
 	gboolean result;
	GConfValue *value;

	g_return_val_if_fail (name != NULL, 0);
	g_return_val_if_fail (preferences_is_initialized (), 0);

	value = preferences_get_value (name);
	result = preferences_gconf_value_get_bool (value);
	eel_gconf_value_free (value);
	
	return result;
}

void
eel_preferences_set_integer (const char *name,
			     int int_value)
{
	char *key;
	int old_value;

	g_return_if_fail (name != NULL);
	g_return_if_fail (preferences_is_initialized ());
	
	key = preferences_key_make (name);
	old_value = eel_preferences_get_integer (name);
	
	if (int_value != old_value) {
		eel_gconf_set_integer (key, int_value);
	}
	g_free (key);
}

int
eel_preferences_get_integer (const char *name)
{
 	int result;
	GConfValue *value;

	g_return_val_if_fail (name != NULL, 0);
	g_return_val_if_fail (preferences_is_initialized (), 0);

	value = preferences_get_value (name);
	result = preferences_gconf_value_get_int (value);
	eel_gconf_value_free (value);

	return result;
}

/* GConf has no unsigned store, save as signed and cast */
guint
eel_preferences_get_uint (const char *name)
{
	return (guint)eel_preferences_get_integer (name);
}
void
eel_preferences_set_uint (const char              *name,
			  guint                    uint_value)
{
	eel_preferences_set_integer (name, (int)uint_value);
}

guint
eel_preferences_get_enum (const char *name)
{
	guint             ret_val;
	char             *str_value;
	GConfValue       *value;
	const EelEnumeration   *enumeration;
	PreferencesEntry *entry;

	g_return_val_if_fail (name != NULL, 0);
	g_return_val_if_fail (preferences_is_initialized (), 0);

	entry = preferences_global_table_lookup_or_insert (name);
	g_return_val_if_fail (entry != NULL, 0);

	enumeration = eel_enumeration_lookup (entry->enumeration_id);

	if (!enumeration) {
		g_warning ("No enum entry for '%s' (%s)",
			   name, entry->enumeration_id);
		return 0;
	}

	value = preferences_get_value (name);
	if (value->type == GCONF_VALUE_INT) { /* compatibility path */
		ret_val = (guint)preferences_gconf_value_get_int (value);
		eel_gconf_value_free (value);
		return ret_val;
	}

	str_value = preferences_gconf_value_get_string (value);
	eel_gconf_value_free (value);

	if (str_value == NULL) {
		g_warning ("No key for '%s' at %s", str_value, name);
		return 0;
	}

	ret_val = eel_enumeration_get_value_for_name (enumeration, str_value);

	g_free (str_value);

	return ret_val;
}

void
eel_preferences_set_enum (const char *name,
			  guint       int_value)
{
	const char       *str_value;
	const EelEnumeration   *enumeration;
	PreferencesEntry *entry;

	g_return_if_fail (name != NULL);
	g_return_if_fail (preferences_is_initialized ());

	entry = preferences_global_table_lookup_or_insert (name);
	g_return_if_fail (entry != NULL);

	enumeration = eel_enumeration_lookup (entry->enumeration_id);

	if (!enumeration) {
		g_warning ("No enum entry for '%s' (%s)",
			   name, entry->enumeration_id);
		return;
	}

	str_value = eel_enumeration_get_name_for_value (enumeration, int_value);

	if (str_value == NULL) {
		g_warning ("No enum match for '%d'", int_value);
		return;
	}

	eel_preferences_set (name, str_value);
}

void
eel_preferences_set (const char *name,
		     const char *string_value)
{
	char *key;
	char *old_value;

	g_return_if_fail (name != NULL);
	g_return_if_fail (preferences_is_initialized ());

	key = preferences_key_make (name);
	old_value = eel_preferences_get (name);

	if (strcmp (string_value, old_value) != 0) {
		eel_gconf_set_string (key, string_value);
	}
	g_free (key);
	g_free (old_value);
}

char *
eel_preferences_get (const char *name)
{
 	char *result;
	GConfValue *value;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (preferences_is_initialized (), NULL);

	value = preferences_get_value (name);
	result = preferences_gconf_value_get_string (value);
	eel_gconf_value_free (value);
	
	return result;
}

void
eel_preferences_set_string_array (const char  *name,
				  char       **strv_value)
{
	GSList *slist;
	int i;
	char *key;

	g_return_if_fail (name != NULL);
	g_return_if_fail (preferences_is_initialized ());

	slist = NULL;
	if (strv_value != NULL) {
		for (i = 0; strv_value[i] != NULL; i++) {
			slist = g_slist_prepend (slist, strv_value[i]);
		}
		slist = g_slist_reverse (slist);
	}

	key = preferences_key_make (name);
	eel_gconf_set_string_list (key, slist);
	g_free (key);

	g_slist_free (slist);
}

static gboolean
string_array_is_valid (char **strv, const char *enumeration_id)
{
	guint i;
	gboolean res;

	g_assert (strv != NULL);
	g_assert (enumeration_id != NULL);

	res = TRUE;
	for (i = 0; strv[i] != NULL; i++) {
		const EelEnumeration *enumeration;

		enumeration = eel_enumeration_lookup (enumeration_id);
		if (!enumeration) {
			res = FALSE;
			break;
		}

		if (!eel_enumeration_contains_name (enumeration, strv[i])) {
			res = FALSE;
			break;
		}
	}

	return res;
}

char **
eel_preferences_get_string_array (const char *name)
{
 	char **result;
	GConfValue *value;
	PreferencesEntry *entry;
	GConfValue *default_value;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (preferences_is_initialized (), NULL);

	value = preferences_get_value (name);
	result = preferences_gconf_value_get_string_array (value);
	eel_gconf_value_free (value);

	entry = preferences_global_table_lookup_or_insert (name);
	g_assert (entry != NULL);

	/* No enumeration_id so we're done */
	if (entry->enumeration_id == NULL) {
		return result;
	}

	/* Do a sanity check on the validity of the values */
	if (string_array_is_valid (result, entry->enumeration_id)) {
		return result;
	}

	/* Forget the bad value and use the default instead */
	g_strfreev (result);

	default_value = preferences_get_default_value (name);
	if (default_value) {		
		result = preferences_gconf_value_get_string_array (default_value);
		gconf_value_free (default_value);
	}

	return result;
}

void
eel_preferences_unset (const char *name)
{
	char *key;

	g_return_if_fail (name != NULL);
	g_return_if_fail (preferences_is_initialized ());

	key = preferences_key_make (name);
	
	eel_gconf_unset (key);

	g_free (key);
}

gboolean
eel_preferences_key_is_writable (const char *name)
{
	gboolean result;
	char *key;
	
	g_return_val_if_fail (name != NULL, 0);
	g_return_val_if_fail (preferences_is_initialized (), 0);

	key = preferences_key_make (name);
	result = eel_gconf_key_is_writable (key);
	g_free (key);

	return result;
}

/**
 * preferences_callback_entry_invoke_function
 *
 * A function that invokes a callback from the given struct.  It is meant to be fed to 
 * g_list_foreach ()
 * @data: The list data privately maintained by the GList.
 * @callback_data: The callback_data privately maintained by the GList.
 **/
static void
preferences_callback_entry_invoke_function (gpointer data,
					    gpointer callback_data)
{
	PreferencesCallbackEntry *callback_entry;
	
	g_assert (data != NULL);
	
	callback_entry = data;

 	(* callback_entry->callback) (callback_entry->callback_data);
}

static void
preferences_entry_invoke_callbacks (PreferencesEntry *entry)
{
	g_assert (entry != NULL);

	/* Update the auto storage preferences */
	if (entry->auto_storage_list != NULL) {
		preferences_entry_update_auto_storage (entry);			
	}
	
	/* Invoke callbacks for this entry if any */
	if (entry->callback_list != NULL) {
		g_list_foreach (entry->callback_list,
				preferences_callback_entry_invoke_function,
				NULL);
	}
}

static void
update_auto_string (gpointer data, gpointer callback_data)
{
	char **storage;
	const char *value;

	g_assert (data != NULL);
	g_assert (callback_data != NULL);

	storage = (char **)data;
	value = (const char *)callback_data;

	g_free (*storage);
	*(char **)storage = g_strdup (value);
}

static void
update_auto_string_array (gpointer data, gpointer callback_data)
{
	char ***storage;
	char **value;
	
	g_assert (data != NULL);
	g_assert (callback_data != NULL);

	storage = (char ***)data;
	value = (char **)callback_data;

	g_strfreev (*storage);
	*(char ***)storage = value ? g_strdupv (value) : NULL;
}

static void
update_auto_string_array_as_quarks (gpointer data, gpointer callback_data)
{
	GQuark **storage;
	char **value;
	int i = 0;
	
	g_assert (data != NULL);
	g_assert (callback_data != NULL);

	storage = (GQuark **)data;
	value = (char **)callback_data;

	g_free (*storage);
	*storage = g_new (GQuark, g_strv_length (value) + 1);

	if (value != NULL) {
		for (i = 0; value[i] != NULL; ++i) {
			(*storage)[i] = g_quark_from_string (value[i]);
		}
	}
	(*storage)[i] = 0;
}

static void
update_auto_integer_or_boolean (gpointer data, gpointer callback_data)
{
	g_assert (data != NULL);

	*(int *)data = GPOINTER_TO_INT (callback_data);
}

static void
preferences_entry_update_auto_storage (PreferencesEntry *entry)
{
	char *new_string_value;
	char **new_string_array_value;
	int new_int_value;
	guint new_uint_value;
	gboolean new_boolean_value;

	switch (entry->type) {
	case PREFERENCE_STRING:
		if (entry->enumeration_id != NULL) {
			new_uint_value = eel_preferences_get_enum (entry->name);
			g_list_foreach (entry->auto_storage_list,
					update_auto_integer_or_boolean,
					GINT_TO_POINTER (new_uint_value));
		} else {
			new_string_value = eel_preferences_get (entry->name);
			g_list_foreach (entry->auto_storage_list,
					update_auto_string,
					new_string_value);
			g_free (new_string_value);
		}
		break;
	case PREFERENCE_STRING_ARRAY:
		new_string_array_value = eel_preferences_get_string_array (entry->name);
		g_list_foreach (entry->auto_storage_list,
				update_auto_string_array,
				new_string_array_value);
		g_strfreev (new_string_array_value);
		break;
	case PREFERENCE_STRING_ARRAY_AS_QUARKS:
		new_string_array_value = eel_preferences_get_string_array (entry->name);
		g_list_foreach (entry->auto_storage_list,
				update_auto_string_array_as_quarks,
				new_string_array_value);
		g_strfreev (new_string_array_value);
		break;
	case PREFERENCE_INTEGER:
		new_int_value = eel_preferences_get_integer (entry->name);
		g_list_foreach (entry->auto_storage_list,
				update_auto_integer_or_boolean,
				GINT_TO_POINTER (new_int_value));
		break;
	case PREFERENCE_BOOLEAN:
		new_boolean_value = eel_preferences_get_boolean (entry->name);
		g_list_foreach (entry->auto_storage_list,
				update_auto_integer_or_boolean,
				GINT_TO_POINTER (new_boolean_value));
		break;
	default:
		g_warning ("unexpected preferences type %d in preferences_entry_update_auto_storage", entry->type);
	}
}

static void
preferences_something_changed_notice (GConfClient *client, 
				      guint connection_id, 
				      GConfEntry *entry, 
				      gpointer notice_data)
{
	g_assert (entry != NULL);
	g_assert (entry->key != NULL);
	g_assert (notice_data != NULL);

	preferences_entry_invoke_callbacks (notice_data);
}

static void
preferences_entry_ensure_gconf_connection (PreferencesEntry *entry)
{
	char *key;
	
	/*
	 * We install only one gconf notification for each preference entry.
	 * Otherwise, we would invoke the installed callbacks more than once
	 * per registered callback.
	 */
	if (entry->gconf_connection_id != EEL_GCONF_UNDEFINED_CONNECTION) {
		return;
	}
		
	g_assert (entry->name != NULL);

	key = preferences_key_make (entry->name);

	entry->gconf_connection_id = eel_gconf_notification_add (key,
								 preferences_something_changed_notice,
								 entry);
	g_free (key);

	g_assert (entry->gconf_connection_id != EEL_GCONF_UNDEFINED_CONNECTION);
}

/**
 * preferences_entry_add_callback
 *
 * Add a callback to a pref node.  Callbacks are fired whenever
 * the pref value changes.
 * @preferences_entry: The hash node.
 * @callback: The user-supplied callback.
 * @callback_data: The user-supplied closure.
 **/
static void
preferences_entry_add_callback (PreferencesEntry *entry,
				EelPreferencesCallback callback,
				gpointer callback_data)
{
	PreferencesCallbackEntry *callback_entry;
	GList *l;

	g_assert (entry != NULL);
	g_assert (callback != NULL);

	callback_entry = g_new0 (PreferencesCallbackEntry, 1);
	callback_entry->callback = callback;
	callback_entry->callback_data = callback_data;

	l = g_list_find_custom (entry->callback_list, callback_entry, preferences_callback_entry_compare);
	if (l == NULL) {
		entry->callback_list = g_list_append (entry->callback_list, callback_entry);
		preferences_entry_ensure_gconf_connection (entry);
	} else {
		g_warning ("Trying to add a callback for %s that already exists.", entry->name);
	}
}

/**
 * preferences_entry_add_auto_storage
 *
 * Add an auto-storage variable to a pref node.  The variable will
 * be updated to match the pref value whenever the pref 
 * the pref value changes.
 * @preferences_entry: The hash node.
 * @storage: The user-supplied location at which to store the value.
 * @type: Which type of variable this is.
 **/
static void
preferences_entry_add_auto_storage (PreferencesEntry *entry,
				    gpointer storage,
				    PreferenceType type)
{
	g_assert (entry != NULL);
	g_assert (storage != NULL);
	g_assert (entry->type == 0 || entry->type == type);
	if (g_list_find (entry->auto_storage_list, storage) != NULL) {
		g_warning ("Trying to add an auto storage for %s that already exists.", entry->name);
		return;
	}

	entry->type = type;
	
	entry->auto_storage_list = g_list_append (entry->auto_storage_list, storage);

	preferences_entry_ensure_gconf_connection (entry);
}

static void
preferences_entry_check_remove_connection (PreferencesEntry *entry)
{
	/*
	 * If there are no callbacks or auto-storage variables left in the entry, 
	 * remove the gconf notification.
	 */
	if (entry->callback_list != NULL || entry->auto_storage_list != NULL) {
		return;
	}

	eel_gconf_notification_remove (entry->gconf_connection_id);
	entry->gconf_connection_id = EEL_GCONF_UNDEFINED_CONNECTION;
}

/**
 * preferences_entry_remove_callback
 *
 * remove a callback from a pref entry.  Both the callback and the callback_data must
 * match in order for a callback to be removed from the entry.
 * @preferences_entry: The hash entry.
 * @callback: The user-supplied callback.
 * @callback_data: The user-supplied closure.
 **/
static void
preferences_entry_remove_callback (PreferencesEntry *entry,
				   EelPreferencesCallback callback,
				   gpointer callback_data)
{
	PreferencesCallbackEntry cb_entry;
	GList *l;

	g_assert (entry != NULL);
	g_assert (callback != NULL);

	cb_entry.callback = callback;
	cb_entry.callback_data = callback_data;

	l = g_list_find_custom (entry->callback_list, &cb_entry, preferences_callback_entry_compare);
	if (l != NULL) {
		preferences_callback_entry_free (l->data);
		entry->callback_list = g_list_delete_link (entry->callback_list, l);
		preferences_entry_check_remove_connection (entry);
	} else {
		g_warning ("Trying to remove a callback for %s without adding it first.", entry->name);
	}

	g_assert (g_list_find_custom (entry->callback_list, &cb_entry, preferences_callback_entry_compare) == NULL);
}

/**
 * preferences_entry_remove_auto_storage
 *
 * remove an auto-storage variable from a pref entry.
 * @preferences_entry: The hash entry.
 * @storage: The user-supplied location.
 **/
static void
preferences_entry_remove_auto_storage (PreferencesEntry *entry,
				       gpointer storage)
{
	GList *new_list;
	const GList *node;
	gpointer storage_in_entry;

	g_assert (entry != NULL);
	g_assert (storage != NULL);
	g_assert (entry->auto_storage_list != NULL);
	
	new_list = g_list_copy (entry->auto_storage_list);
	
	for (node = new_list; node != NULL; node = node->next) {
		storage_in_entry = node->data;
		
		g_assert (storage_in_entry != NULL);
		
		if (storage_in_entry == storage) {
			entry->auto_storage_list = g_list_remove (entry->auto_storage_list, 
							          storage);

			switch (entry->type) {
			case PREFERENCE_STRING:
				update_auto_string (storage, NULL);
				break;
			case PREFERENCE_STRING_ARRAY:
				update_auto_string_array (storage, NULL);
				break;
			case PREFERENCE_STRING_ARRAY_AS_QUARKS:
				update_auto_string_array_as_quarks (storage, NULL);
				break;
			case PREFERENCE_BOOLEAN:
			case PREFERENCE_INTEGER:
				update_auto_integer_or_boolean (storage, NULL);
				break;
			default:
				g_warning ("unexpected preference type %d in preferences_entry_remove_auto_storage", entry->type);
			}
		}
	}

	g_list_free (new_list);

	preferences_entry_check_remove_connection (entry);
}

/**
 * preferences_callback_entry_free
 *
 * Free a callback info struct.
 * @preferences_callback_entry: The struct to free.
 **/
static void
preferences_callback_entry_free (PreferencesCallbackEntry *callback_entry)
{
	g_assert (callback_entry != NULL);

	callback_entry->callback = NULL;
	callback_entry->callback_data = NULL;

	g_free (callback_entry);
}

/**
 * preferences_callback_entry_free_func
 *
 * A function that frees a callback info struct.  It is meant to be fed to 
 * g_list_foreach ()
 * @data: The list data privately maintained by the GList.
 * @callback_data: The callback_data privately maintained by the GList.
 **/
static void
preferences_callback_entry_free_func (gpointer	data,
				      gpointer	callback_data)
{
	g_assert (data != NULL);
	
	preferences_callback_entry_free (data);
}

/**
 * preferences_entry_free
 *
 * Free a preference hash node's members along with the node itself.
 * @preferences_hash_node: The node to free.
 **/
static void
preferences_entry_free (PreferencesEntry *entry)
{
	g_assert (entry != NULL);

	eel_gconf_notification_remove (entry->gconf_connection_id);
	entry->gconf_connection_id = EEL_GCONF_UNDEFINED_CONNECTION;

	g_list_free (entry->auto_storage_list);
	eel_g_list_free_deep_custom (entry->callback_list,
				     preferences_callback_entry_free_func,
				     NULL);
	
	entry->auto_storage_list = NULL;
	entry->callback_list = NULL;

	g_free (entry->name);
	g_free (entry->description);
	g_free (entry->enumeration_id);

	eel_gconf_value_free (entry->fallback);

	g_free (entry);
}

/**
 * preferences_entry_free_func
 *
 * A function that frees a pref hash node.  It is meant to be fed to 
 * g_hash_table_foreach ()
 * @key: The hash key privately maintained by the GHashTable.
 * @value: The hash value privately maintained by the GHashTable.
 * @callback_data: The callback_data privately maintained by the GHashTable.
 **/
static void
preferences_entry_free_func (gpointer key,
			     gpointer value,
			     gpointer callback_data)
{
	g_assert (value != NULL);

	preferences_entry_free (value);
}

static void
preferences_global_table_free (void)
{
	if (global_table == NULL) {
		return;
	}
	
	g_hash_table_foreach (global_table, preferences_entry_free_func, NULL);
	g_hash_table_destroy (global_table);
	global_table = NULL;

	g_free (storage_path);
	storage_path = NULL;
}

static void
preferences_uninitialize (void)
{
	initialized = FALSE;
}

static GHashTable *
preferences_global_table_get_global (void)
{
	static gboolean at_exit_handler_added = FALSE;

	if (global_table == NULL) {
		global_table = g_hash_table_new (g_str_hash, g_str_equal);

		if (!at_exit_handler_added) {
			at_exit_handler_added = TRUE;
			eel_debug_call_at_shutdown (preferences_global_table_free);
			/* ensure that we catch calls to preferences functions after eel shutdown */
			eel_debug_call_at_shutdown (preferences_uninitialize);
		}
	}
	
	return global_table;
}

static PreferencesEntry *
preferences_global_table_lookup (const char *name)
{
	g_assert (name != NULL);
	g_assert (preferences_global_table_get_global () != NULL);
	
	return g_hash_table_lookup (preferences_global_table_get_global (), name);
}

static PreferencesEntry *
preferences_global_table_insert (const char *name)
{
	PreferencesEntry *entry;

	g_assert (name != NULL);
	g_assert (preferences_global_table_get_global () != NULL);
	g_assert (preferences_global_table_lookup (name) == NULL);
	
	entry = g_new0 (PreferencesEntry, 1);
	entry->name = g_strdup (name);

	g_hash_table_insert (preferences_global_table_get_global (), entry->name, entry);

	g_assert (entry == preferences_global_table_lookup (name));

	return entry;
}

static PreferencesEntry *
preferences_global_table_lookup_or_insert (const char *name)
{
	PreferencesEntry *entry;

	g_assert (name != NULL);
	
	entry = preferences_global_table_lookup (name);

	if (entry != NULL) {
		return entry;
	}

	entry = preferences_global_table_insert (name);
	g_assert (entry != NULL);

	return entry;
}

void
eel_preferences_add_callback (const char *name,
			      EelPreferencesCallback callback,
			      gpointer callback_data)
{
	PreferencesEntry *entry;

	g_return_if_fail (name != NULL);
	g_return_if_fail (callback != NULL);
	g_return_if_fail (preferences_is_initialized ());

	entry = preferences_global_table_lookup_or_insert (name);
	g_assert (entry != NULL);

	preferences_entry_add_callback (entry, callback, callback_data);
}

void
eel_preferences_add_auto_string (const char *name,
				 const char **storage)
{
	PreferencesEntry *entry;
	char *value;

	g_return_if_fail (name != NULL);
	g_return_if_fail (storage != NULL);
	g_return_if_fail (preferences_is_initialized ());

	entry = preferences_global_table_lookup_or_insert (name);
	g_assert (entry != NULL);

	preferences_entry_add_auto_storage (entry, storage, PREFERENCE_STRING);

	value = eel_preferences_get (entry->name);
	update_auto_string (storage, value);
	g_free (value);
}

void
eel_preferences_add_auto_string_array (const char   *name,
				       char       ***storage)
{
	PreferencesEntry *entry;
	char **value;

	g_return_if_fail (name != NULL);
	g_return_if_fail (storage != NULL);
	g_return_if_fail (preferences_is_initialized ());

	entry = preferences_global_table_lookup_or_insert (name);
	g_assert (entry != NULL);

	preferences_entry_add_auto_storage (entry, storage, PREFERENCE_STRING_ARRAY);

	value = eel_preferences_get_string_array (entry->name);
	update_auto_string_array (storage, value);
	g_strfreev (value);
}

void
eel_preferences_add_auto_string_array_as_quarks (const char *name,
						 GQuark **storage)
{
	PreferencesEntry *entry;
	char **value;

	g_return_if_fail (name != NULL);
	g_return_if_fail (storage != NULL);
	g_return_if_fail (preferences_is_initialized ());

	entry = preferences_global_table_lookup_or_insert (name);
	g_assert (entry != NULL);

	preferences_entry_add_auto_storage (entry, storage, PREFERENCE_STRING_ARRAY_AS_QUARKS);

	value = eel_preferences_get_string_array (entry->name);
	update_auto_string_array_as_quarks (storage, value);
	g_strfreev (value);
}

void
eel_preferences_add_auto_integer (const char *name,
				  int *storage)
{
	PreferencesEntry *entry;
	int value;

	g_return_if_fail (name != NULL);
	g_return_if_fail (storage != NULL);
	g_return_if_fail (preferences_is_initialized ());

	entry = preferences_global_table_lookup_or_insert (name);
	g_assert (entry != NULL);

	preferences_entry_add_auto_storage (entry, storage, PREFERENCE_INTEGER);

	value = eel_preferences_get_integer (entry->name);
	update_auto_integer_or_boolean (storage, GINT_TO_POINTER (value));
}


void
eel_preferences_add_auto_enum (const char *name,
			       guint *storage)
{
	PreferencesEntry *entry;
	guint value;

	g_return_if_fail (name != NULL);
	g_return_if_fail (storage != NULL);
	g_return_if_fail (preferences_is_initialized ());

	entry = preferences_global_table_lookup_or_insert (name);
	g_assert (entry != NULL);
	g_assert (entry->enumeration_id != NULL);

	preferences_entry_add_auto_storage (entry, storage, PREFERENCE_STRING);

	value = eel_preferences_get_enum (entry->name);
	update_auto_integer_or_boolean (storage, GINT_TO_POINTER (value));
}

void
eel_preferences_add_auto_boolean (const char *name,
				  gboolean *storage)
{
	PreferencesEntry *entry;
	gboolean value;

	g_return_if_fail (name != NULL);
	g_return_if_fail (storage != NULL);
	g_return_if_fail (preferences_is_initialized ());

	entry = preferences_global_table_lookup_or_insert (name);
	g_assert (entry != NULL);

	preferences_entry_add_auto_storage (entry, storage, PREFERENCE_BOOLEAN);

	value = eel_preferences_get_boolean (entry->name);
	update_auto_integer_or_boolean (storage, GINT_TO_POINTER (value));
}

void
eel_preferences_remove_auto_string (const char *name,
				    const char **storage)
{
	PreferencesEntry *entry;

	g_return_if_fail (name != NULL);
	g_return_if_fail (storage != NULL);
	g_return_if_fail (preferences_is_initialized ());

	entry = preferences_global_table_lookup (name);
	if (entry == NULL) {
		g_warning ("Trying to remove auto-string for %s without adding it first.", name);
		return;
	}

	preferences_entry_remove_auto_storage (entry, storage);
}

void
eel_preferences_remove_auto_string_array (const char *name,
					  char ***storage)
{
	PreferencesEntry *entry;

	g_return_if_fail (name != NULL);
	g_return_if_fail (storage != NULL);
	g_return_if_fail (preferences_is_initialized ());

	entry = preferences_global_table_lookup (name);
	if (entry == NULL) {
		g_warning ("Trying to remove auto-string for %s without adding it first.", name);
		return;
	}

	preferences_entry_remove_auto_storage (entry, storage);
}

void
eel_preferences_remove_auto_integer (const char *name,
				     int *storage)
{
	PreferencesEntry *entry;

	g_return_if_fail (name != NULL);
	g_return_if_fail (storage != NULL);
	g_return_if_fail (preferences_is_initialized ());

	entry = preferences_global_table_lookup (name);
	if (entry == NULL) {
		g_warning ("Trying to remove auto-integer for %s without adding it first.", name);
		return;
	}

	preferences_entry_remove_auto_storage (entry, storage);
}

void
eel_preferences_remove_auto_boolean (const char *name,
				     gboolean *storage)
{
	PreferencesEntry *entry;

	g_return_if_fail (name != NULL);
	g_return_if_fail (storage != NULL);
	g_return_if_fail (preferences_is_initialized ());

	entry = preferences_global_table_lookup (name);
	if (entry == NULL) {
		g_warning ("Trying to remove auto-boolean for %s without adding it first.", name);
		return;
	}

	preferences_entry_remove_auto_storage (entry, storage);
}

typedef struct
{
	char *name;
	EelPreferencesCallback callback;
	gpointer callback_data;
} WhileAliveData;

static void
preferences_while_alive_disconnector (gpointer callback_data, GObject *where_object_was)
{
	WhileAliveData *data;

	g_assert (callback_data != NULL);

	data = callback_data;

	/* we might have survived an eel shutdown, which
	 * already cleared all the callbacks */
	if (preferences_is_initialized ()) {
		eel_preferences_remove_callback (data->name,
						 data->callback,
						 data->callback_data);
	}

	g_free (data->name);
	g_free (data);
}

void
eel_preferences_add_callback_while_alive (const char *name,
					  EelPreferencesCallback callback,
					  gpointer callback_data,
					  GObject *alive_object)
{
	WhileAliveData *data;

	g_return_if_fail (name != NULL);
	g_return_if_fail (callback != NULL);
	g_return_if_fail (G_IS_OBJECT (alive_object));
	g_return_if_fail (preferences_is_initialized ());

	data = g_new (WhileAliveData, 1);
	data->name = g_strdup (name);
	data->callback = callback;
	data->callback_data = callback_data;

	eel_preferences_add_callback (name, callback, callback_data);

	g_object_weak_ref (alive_object,
			   preferences_while_alive_disconnector,
			   data);
}

void
eel_preferences_remove_callback (const char *name,
				 EelPreferencesCallback callback,
				 gpointer callback_data)
{
	PreferencesEntry *entry;

	g_return_if_fail (name != NULL);
	g_return_if_fail (callback != NULL);
	g_return_if_fail (preferences_is_initialized ());

	entry = preferences_global_table_lookup (name);

	if (entry == NULL) {
		g_warning ("Trying to remove a callback for %s without adding it first.", name);
		return;
	}
	
	preferences_entry_remove_callback (entry, callback, callback_data);
}

void
eel_preferences_set_description (const char *name,
				 const char *description)
{
	PreferencesEntry *entry;

	g_return_if_fail (name != NULL);
	g_return_if_fail (description != NULL);
	g_return_if_fail (preferences_is_initialized ());

	entry = preferences_global_table_lookup_or_insert (name);
	g_assert (entry != NULL);

	g_free (entry->description);
	entry->description = g_strdup (description);
}

char *
eel_preferences_get_description (const char *name)
{
	PreferencesEntry *entry;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (preferences_is_initialized (), NULL);

	entry = preferences_global_table_lookup_or_insert (name);

	return g_strdup (entry->description ? entry->description : "");
}

void
eel_preferences_set_enumeration_id (const char *name,
				    const char *enumeration_id)
{
	PreferencesEntry *entry;

	g_return_if_fail (name != NULL);
	g_return_if_fail (enumeration_id != NULL);
	g_return_if_fail (preferences_is_initialized ());

	entry = preferences_global_table_lookup_or_insert (name);
	g_assert (entry != NULL);

	g_free (entry->enumeration_id);
	entry->enumeration_id = g_strdup (enumeration_id);
}

char *
eel_preferences_get_enumeration_id (const char *name)
{
	PreferencesEntry *entry;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (preferences_is_initialized (), NULL);

	entry = preferences_global_table_lookup_or_insert (name);

	return g_strdup (entry->enumeration_id);
}

static void
preferences_set_emergency_fallback_stealing_value (const char *name,
						   GConfValue *value)
{
	PreferencesEntry *entry;

	g_assert (name != NULL);
	g_assert (preferences_is_initialized ());

	entry = preferences_global_table_lookup_or_insert (name);
	g_assert (entry != NULL);

	if (entry->fallback)
		gconf_value_free (entry->fallback);
	entry->fallback = value; /* steal ownership of value */
}

void
eel_preferences_set_emergency_fallback_string (const char *name,
					       const char *value)
{
	GConfValue *gconf_value;

	g_return_if_fail (name != NULL);
	g_return_if_fail (value != NULL);
	
	gconf_value = gconf_value_new (GCONF_VALUE_STRING);

	gconf_value_set_string (gconf_value, value);

	preferences_set_emergency_fallback_stealing_value (name, gconf_value);
}

void
eel_preferences_set_emergency_fallback_integer (const char *name,
						int         value)
{
	GConfValue *gconf_value;

	g_return_if_fail (name != NULL);
	
	gconf_value = gconf_value_new (GCONF_VALUE_INT);

	gconf_value_set_int (gconf_value, value);

	preferences_set_emergency_fallback_stealing_value (name, gconf_value);
}

void
eel_preferences_set_emergency_fallback_boolean (const char *name,
						gboolean    value)
{
	GConfValue *gconf_value;

	g_return_if_fail (name != NULL);
	
	gconf_value = gconf_value_new (GCONF_VALUE_BOOL);

	gconf_value_set_bool (gconf_value, value);

	preferences_set_emergency_fallback_stealing_value (name, gconf_value);
}


void
eel_preferences_set_emergency_fallback_string_array (const char  *name,
						     char       **value)
{
	GConfValue *gconf_value;
	GSList *list;
	int i;

	g_return_if_fail (name != NULL);
	g_return_if_fail (value != NULL);

	gconf_value = gconf_value_new (GCONF_VALUE_LIST);
	gconf_value_set_list_type (gconf_value, GCONF_VALUE_STRING);

	list = NULL;
	for (i = 0; value[i] != NULL; ++i)
	{
		GConfValue *v;

		v = gconf_value_new (GCONF_VALUE_STRING);
		gconf_value_set_string (v, value[i]);

		list = g_slist_prepend (list, v);
	}

	gconf_value_set_list_nocopy (gconf_value, g_slist_reverse (list));

	preferences_set_emergency_fallback_stealing_value (name, gconf_value);
}

GConfValue*
eel_preferences_get_emergency_fallback (const char *name)
{
	PreferencesEntry *entry;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (preferences_is_initialized (), NULL);

	entry = preferences_global_table_lookup_or_insert (name);
	
	return entry->fallback ? gconf_value_copy (entry->fallback) : NULL;
}

gboolean
eel_preferences_monitor_directory (const char *directory)
{
	g_return_val_if_fail (preferences_is_initialized (), FALSE);

	return eel_gconf_monitor_add (directory);
}

gboolean
eel_preferences_is_visible (const char *name)
{
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (preferences_is_initialized (), FALSE);

	return !preferences_global_table_lookup_or_insert (name)->invisible;
}

void
eel_preferences_init (const char *path)
{
	g_return_if_fail (eel_strlen (path) > 0);
	
	if (initialized) {
		return;
	}

	initialized = TRUE;

	preferences_set_storage_path (path);
}

#if !defined (EEL_OMIT_SELF_CHECK)

#define CHECK_BOOLEAN(name__, value__)								\
G_STMT_START {											\
	eel_preferences_set_boolean ((name__), (value__));					\
	EEL_CHECK_BOOLEAN_RESULT (eel_preferences_get_boolean (name__), (value__));	\
} G_STMT_END

#define CHECK_INTEGER(name__, value__)								\
G_STMT_START {											\
	eel_preferences_set_integer ((name__), (value__));					\
	EEL_CHECK_INTEGER_RESULT (eel_preferences_get_integer (name__), (value__));	\
} G_STMT_END

#define CHECK_STRING(name__, value__)							\
G_STMT_START {										\
	eel_preferences_set ((name__), (value__));					\
	EEL_CHECK_STRING_RESULT (eel_preferences_get (name__), (value__));		\
} G_STMT_END

void
eel_self_check_preferences (void)
{
	/* Disabled until I can debug why these seemingly harmless tests 
	 * dont work. -re
	 */
#if 0
	int original_user_level;

	original_user_level = eel_preferences_get_user_level ();

 	EEL_CHECK_INTEGER_RESULT (eel_preferences_get_integer ("self-check/neverset/i"), 0);
 	EEL_CHECK_STRING_RESULT (eel_preferences_get ("self-check/neverset/s"), "");
 	EEL_CHECK_BOOLEAN_RESULT (eel_preferences_get_boolean ("self-check/neverset/b"), FALSE);

	eel_preferences_set_user_level (0);

	/* FIXME: Fails if you add the commented-out lines. */
	/* CHECK_INTEGER ("self-check/i", 0); */
	CHECK_INTEGER ("self-check/i", 666);
	/* CHECK_BOOLEAN ("self-check/b", FALSE); */
	CHECK_BOOLEAN ("self-check/b", TRUE);
	/* CHECK_STRING ("self-check/s", ""); */
	CHECK_STRING ("self-check/s", "foo");

	/* Restore the original user level */
	eel_preferences_set_user_level (original_user_level);
#endif
}

#endif /* !EEL_OMIT_SELF_CHECK */
