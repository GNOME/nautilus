/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-preferences.c - Preference peek/poke/notify implementation.

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

#include "nautilus-gconf-extensions.h"
#include "nautilus-lib-self-check-functions.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-string-list.h>
#include <eel/eel-string.h>
#include <gconf/gconf-client.h>
#include <gconf/gconf.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>

#define DEFAULT_USER_LEVEL	NAUTILUS_USER_LEVEL_INTERMEDIATE

/* An enumeration used for updating auto-storage variables in a type-specific way. 
 * FIXME: there is another enumeration like this in nautilus-global-preferences.c,
 * used for different purposes but in a related way. Should we combine them?
 */
typedef enum
{
	PREFERENCE_BOOLEAN = 1,
	PREFERENCE_INTEGER,
	PREFERENCE_STRING
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
	GList *callback_list;
	GList *auto_storage_list;
	int gconf_connection_id;
	char *enumeration_id;
	GConfValue *cached_value;
} PreferencesEntry;

/*
 * PreferencesCallbackEntry:
 *
 * A structure to manage callback lists.  A callback list is a GList.
 * The callback_data in each list node is a pointer to the following 
 * struct:
 */
typedef struct {
	NautilusPreferencesCallback callback;
	gpointer callback_data;
} PreferencesCallbackEntry;


static const char *user_level_names_for_display[] =
{
	N_("Beginner"),
	N_("Intermediate"),
	N_("Advanced")
};

static const char *user_level_names_for_storage[] =
{
	"novice",
	"intermediate",
	"advanced"
};

static char *       preferences_get_path                            (void);
static char *       preferences_get_defaults_path                   (void);
static char *       preferences_get_visibility_path                 (void);
static char *       preferences_get_user_level_key                  (void);
static GConfClient *preferences_global_client_get                   (void);
static void         preferences_global_client_remove_notification   (void);
static gboolean     preferences_preference_is_internal              (const char               *name);
static gboolean     preferences_preference_is_user_level            (const char               *name);
static gboolean     preferences_preference_is_default               (const char               *name);
static char *       preferences_key_make                            (const char               *name);
static char *       preferences_key_make_for_getter                 (const char               *name);
static char *       preferences_key_make_for_default                (const char               *name,
								     int                       user_level);
static char *       preferences_key_make_for_default_getter         (const char               *name,
								     int                       user_level);
static char *       preferences_key_make_for_visibility             (const char               *name);
static void         preferences_user_level_changed_notice           (GConfClient              *client,
								     guint                     connection_id,
								     GConfEntry               *gconf_entry,
								     gpointer                  user_data);
static void         preferences_something_changed_notice            (GConfClient              *client,
								     guint                     connection_id,
								     GConfEntry               *gconf_entry,
								     gpointer                  user_data);
static void         preferences_global_table_check_changes_function (gpointer                  key,
								     gpointer                  value,
								     gpointer                  callback_data);
static GHashTable  *preferences_global_table_get_global             (void);
static void         preferences_callback_entry_free                 (PreferencesCallbackEntry *callback_entry);
static int          preferences_user_level_check_range              (int                       user_level);
static void         preferences_entry_update_auto_storage           (PreferencesEntry         *entry);

static int user_level_changed_connection_id = -1;
static GHashTable *global_table = NULL;

static char *
preferences_get_path (void)
{
	return g_strdup ("/apps/nautilus");
}

static char *
preferences_get_defaults_path (void)
{
	char *defaults_path;
	char *path;
	
	path = preferences_get_path ();
	defaults_path = g_strdup_printf ("%s/defaults", path);
	g_free (path);
	return defaults_path;
}

static char *
preferences_get_visibility_path (void)
{
	char *visibility_path;
	char *path;
	
	path = preferences_get_path ();
	visibility_path = g_strdup_printf ("%s/visibility", path);
	g_free (path);

	return visibility_path;
}

static char *
preferences_get_user_level_key (void)
{
	char *user_level_key;
	char *path;

	path = preferences_get_path ();
	user_level_key = g_strdup_printf ("%s/user_level", path);
	g_free (path);

	return user_level_key;
}

/* If the preference name begind with a "/", we interpret 
 * it as a straight gconf key. */
static gboolean
preferences_preference_is_internal (const char *name)
{
	g_return_val_if_fail (name != NULL, FALSE);
	
	if (eel_str_has_prefix (name, "/")) {
		return FALSE;
	}
	
	return TRUE;
}

static gboolean
preferences_preference_is_user_level (const char *name)
{
	gboolean result;
	char *user_level_key;

	g_return_val_if_fail (name != NULL, FALSE);

	user_level_key = preferences_get_user_level_key ();

	result = eel_str_is_equal (name, user_level_key)
		|| eel_str_is_equal (name, "user_level");

	g_free (user_level_key);

	return result;
}

static char *
preferences_key_make (const char *name)
{
	char *key;
	char *path;

	g_return_val_if_fail (name != NULL, NULL);
	
	if (!preferences_preference_is_internal (name)) {
		return g_strdup (name);
	}

	/* Otherwise, we prefix it with the path */
	path = preferences_get_path ();
	key = g_strdup_printf ("%s/%s", path, name);
	g_free (path);

	return key;
}

static char *
preferences_key_make_for_default (const char *name,
				  int user_level)
{
	char *key;
	char *default_key = NULL;
	char *defaults_path;
	char *storage_name;

	g_return_val_if_fail (name != NULL, NULL);

	user_level = preferences_user_level_check_range (user_level);

	key = preferences_key_make (name);
	defaults_path = preferences_get_defaults_path ();

	storage_name = nautilus_preferences_get_user_level_name_for_storage (user_level);
	default_key = g_strdup_printf ("%s/%s%s",
				       defaults_path,
				       storage_name,
				       key);
	g_free (storage_name);
	g_free (key);
	g_free (defaults_path);
	
	return default_key;
}

static char *
preferences_key_make_for_default_getter (const char *name,
					 int user_level)
{
	char *default_key_for_getter = NULL;
	gboolean done;

	g_return_val_if_fail (name != NULL, NULL);

	user_level = preferences_user_level_check_range (user_level);

	done = FALSE;
	while (!done) {
		default_key_for_getter = preferences_key_make_for_default (name, user_level);
		
		done = (user_level == 0) || (!nautilus_gconf_is_default (default_key_for_getter));
		
		if (!done) {
			g_free (default_key_for_getter);
			user_level--;
		}
	}

	return default_key_for_getter;
}

static char *
preferences_key_make_for_visibility (const char *name)
{
	char *default_key;
	char *key;
	char *visibility_path;

	g_return_val_if_fail (name != NULL, NULL);

	key = preferences_key_make (name);

	visibility_path = preferences_get_visibility_path ();
	default_key = g_strdup_printf ("%s%s", visibility_path, key);
	g_free (key);
	g_free (visibility_path);
	
	return default_key;
}

static void
preferences_global_client_remove_notification (void)
{
	GConfClient *client;

	client = preferences_global_client_get ();

	g_return_if_fail (client != NULL);

	gconf_client_notify_remove (client, user_level_changed_connection_id);
	user_level_changed_connection_id = -1;
}

static GConfClient *
preferences_global_client_get (void)
{
	static GConfClient *global_gconf_client = NULL;
	GError *error = NULL;
	char *path;
	char *user_level_key;
	
	if (global_gconf_client != NULL) {
		return global_gconf_client;
	}

	global_gconf_client = nautilus_gconf_client_get_global ();
	
	g_return_val_if_fail (global_gconf_client != NULL, NULL);
	
	user_level_key = preferences_get_user_level_key ();
	error = NULL;
	user_level_changed_connection_id = gconf_client_notify_add (global_gconf_client,
								    user_level_key,
								    preferences_user_level_changed_notice,
								    NULL,
								    NULL,
								    &error);
	g_free (user_level_key);

	if (nautilus_gconf_handle_error (&error)) {
		global_gconf_client = NULL;
		return NULL;
	}

	path = preferences_get_path ();
	nautilus_gconf_monitor_directory (path);
	g_free (path);
	
	g_atexit (preferences_global_client_remove_notification);

	return global_gconf_client;
}

static gboolean
preferences_preference_is_default (const char *name)
{
	gboolean result;
	char *key;
	
	g_return_val_if_fail (name != NULL, FALSE);
	
	key = preferences_key_make (name);
	result = nautilus_gconf_is_default (key);
	g_free (key);

	return result;
}

static char *
preferences_make_user_level_filtered_key (const char *name)
{
	char *key;
	
	g_return_val_if_fail (name != NULL, NULL);

	if (nautilus_preferences_is_visible (name)) {
		key = preferences_key_make (name);
	} else {
		key = preferences_key_make_for_default (name, nautilus_preferences_get_user_level ());
	}

	return key;
}

/* Public preferences functions */
int
nautilus_preferences_get_visible_user_level (const char *name)
{
  	int result;
	char *visible_key;
	
	g_return_val_if_fail (name != NULL, FALSE);
	
	visible_key = preferences_key_make_for_visibility (name);
	result = nautilus_gconf_get_integer (visible_key);
	g_free (visible_key);

	return result;
}

void
nautilus_preferences_set_visible_user_level (const char *name,
					     int visible_user_level)
{
	char *visible_key;
	
	g_return_if_fail (name != NULL);
	
	visible_key = preferences_key_make_for_visibility (name);
	nautilus_gconf_set_integer (visible_key, visible_user_level);
	g_free (visible_key);
}

void
nautilus_preferences_set_boolean (const char *name,
				  gboolean boolean_value)
{
	char *key;

	g_return_if_fail (name != NULL);
	
	key = preferences_key_make (name);
	nautilus_gconf_set_boolean (key, boolean_value);
	g_free (key);

	nautilus_gconf_suggest_sync ();
}

static char *
preferences_key_make_for_getter (const char *name)
{
	char *key;

	g_return_val_if_fail (name != NULL, NULL);

	if (preferences_preference_is_default (name) || !nautilus_preferences_is_visible (name)) {
		key = preferences_key_make_for_default_getter (name, nautilus_preferences_get_user_level ());
	} else {
		key = preferences_make_user_level_filtered_key (name);
	}
	
	return key;
}

gboolean
nautilus_preferences_get_boolean (const char *name)
{
 	gboolean result;
	char *key;
	
	g_return_val_if_fail (name != NULL, FALSE);

	key = preferences_key_make_for_getter (name);
	result = nautilus_gconf_get_boolean (key);
	g_free (key);

	return result;
}

void
nautilus_preferences_set_integer (const char *name,
				  int int_value)
{
	char *key;
	int old_value;

	g_return_if_fail (name != NULL);
	
	key = preferences_key_make (name);
	old_value = nautilus_preferences_get_integer (name);

	if (int_value != old_value) {
		nautilus_gconf_set_integer (key, int_value);

		nautilus_gconf_suggest_sync ();
	}
	g_free (key);
}

int
nautilus_preferences_get_integer (const char *name)
{
 	int result;
	char *key;

	g_return_val_if_fail (name != NULL, 0);

	key = preferences_key_make_for_getter (name);
	result = nautilus_gconf_get_integer (key);

	g_free (key);

	return result;
}

void
nautilus_preferences_set (const char *name,
			  const char *string_value)
{
	char *key;
	char *old_value;

	g_return_if_fail (name != NULL);

	key = preferences_key_make (name);
	old_value = nautilus_preferences_get (name);

	if (strcmp (string_value, old_value) != 0) {
		nautilus_gconf_set_string (key, string_value);
		
		nautilus_gconf_suggest_sync ();
	}
	g_free (key);
}

char *
nautilus_preferences_get (const char *name)
{
 	char *result;
	char *key;

	g_return_val_if_fail (name != NULL, NULL);

	key = preferences_key_make_for_getter (name);
	result = nautilus_gconf_get_string (key);
	g_free (key);

	if (result == NULL) {
		result = g_strdup ("");
	}

	return result;
}

void
nautilus_preferences_set_string_list (const char *name,
				      GList *string_list_value)
{
	char *key;

	g_return_if_fail (name != NULL);
	
	key = preferences_key_make (name);
	nautilus_gconf_set_string_list (key, string_list_value);
	g_free (key);

	nautilus_gconf_suggest_sync ();
}

GList *
nautilus_preferences_get_string_list (const char *name)
{
 	GList *result;
	char *key;
	
	g_return_val_if_fail (name != NULL, NULL);
	
	key = preferences_key_make_for_getter (name);
	result = nautilus_gconf_get_string_list (key);
	g_free (key);

	return result;
}

int
nautilus_preferences_get_user_level (void)
{
	char *key;
	char *user_level;
	int result;

	/* This is a little silly, but it is technically possible
	 * to have different user_level defaults in each user level.
	 *
	 * This is a consequence of using gconf to store the user
	 * level itself.  So, we special case the "user_level" setting
	 * to always return the default for the first user level.
	 */
	if (preferences_preference_is_default ("user_level")) {
		key = preferences_key_make_for_default ("user_level", 0);
	} else {
		key = preferences_key_make ("user_level");
	}

	user_level = nautilus_gconf_get_string (key);
	g_free (key);

	if (eel_str_is_equal (user_level, "advanced")) {
		result = NAUTILUS_USER_LEVEL_ADVANCED;
	} else if (eel_str_is_equal (user_level, "intermediate")) {
		result = NAUTILUS_USER_LEVEL_INTERMEDIATE;
	} else if (eel_str_is_equal (user_level, "novice")) {
		result = NAUTILUS_USER_LEVEL_NOVICE;
	} else {
		result = DEFAULT_USER_LEVEL;
	}
	
	g_free (user_level);
	return result;
}

void
nautilus_preferences_set_user_level (int user_level)
{
	char *user_level_key;

	user_level = preferences_user_level_check_range (user_level);

	user_level_key = preferences_get_user_level_key ();
	nautilus_gconf_set_string (user_level_key, user_level_names_for_storage[user_level]);
	g_free (user_level_key);

	nautilus_gconf_suggest_sync ();
}

void
nautilus_preferences_default_set_integer (const char *name,
					  int user_level,
					  int int_value)
{
	char *default_key;

	g_return_if_fail (name != NULL);
	
	default_key = preferences_key_make_for_default (name, user_level);
	nautilus_gconf_set_integer (default_key, int_value);
	g_free (default_key);
}

int
nautilus_preferences_default_get_integer (const char *name,
					  int user_level)
{
 	int result;
	char *default_key;

	g_return_val_if_fail (name != NULL, 0);
	
	default_key = preferences_key_make_for_default (name, user_level);
	result = nautilus_gconf_get_integer (default_key);
	g_free (default_key);

	return result;
}

void
nautilus_preferences_default_set_boolean (const char *name,
					  int user_level,
					  gboolean boolean_value)
{
	char *default_key;
	
	g_return_if_fail (name != NULL);
	
	default_key = preferences_key_make_for_default (name, user_level);
	nautilus_gconf_set_boolean (default_key, boolean_value);
	g_free (default_key);
}

gboolean
nautilus_preferences_default_get_boolean (const char *name,
					  int user_level)
{
 	gboolean result;
	char *default_key;

	g_return_val_if_fail (name != NULL, FALSE);
	
	default_key = preferences_key_make_for_default (name, user_level);
	result = nautilus_gconf_get_boolean (default_key);
	g_free (default_key);

	return result;
}

void
nautilus_preferences_default_set_string (const char *name,
					 int user_level,
					 const char *string_value)
{
	char *default_key;
	
	g_return_if_fail (name != NULL);
	
	default_key = preferences_key_make_for_default (name, user_level);
	nautilus_gconf_set_string (default_key, string_value);
	g_free (default_key);
}

char *
nautilus_preferences_default_get_string (const char *name,
					 int user_level)
{
 	char *result;
	char *default_key;

	g_return_val_if_fail (name != NULL, NULL);
	
	default_key = preferences_key_make_for_default (name, user_level);
	result = nautilus_gconf_get_string (default_key);
	g_free (default_key);

	return result;
}

void
nautilus_preferences_default_set_string_list (const char *name,
					      int user_level,
					      GList *string_list_value)
{
	char *default_key;
	
	g_return_if_fail (name != NULL);
	
	default_key = preferences_key_make_for_default (name, user_level);
	nautilus_gconf_set_string_list (default_key, string_list_value);
	g_free (default_key);
}

GList *
nautilus_preferences_default_get_string_list (const char *name,
					      int user_level)
{
 	GList *result;
	char *default_key;
	
	g_return_val_if_fail (name != NULL, NULL);
	
	default_key = preferences_key_make_for_default (name, user_level);
	result = nautilus_gconf_get_string_list (default_key);
	g_free (default_key);

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
	
	g_return_if_fail (data != NULL);
	
	callback_entry = data;

 	(* callback_entry->callback) (callback_entry->callback_data);
}

/**
 * preferences_entry_invoke_callbacks_if_needed
 *
 * @entry: A PreferencesEntry
 *
 * This function checks the cached value in the entry with the current
 * value of the preference.  If the value has changed, then callbacks
 * are invoked and auto storage updated.
 *
 * We need this check because even though the GConf value of a preference
 * could indeed have changed, its representation on the Nautilus side
 * of things could still be the same.  The best example of this is 
 * user level changes, where the value of the preference on the Nautilus
 * end of things is determined by visibility.
 **/
static void
preferences_entry_invoke_callbacks_if_needed (PreferencesEntry *entry)
{
	GConfValue *new_value;
	char *getter_key;
	
	g_return_if_fail (entry != NULL);

	getter_key = preferences_key_make_for_getter (entry->name);
	new_value = nautilus_gconf_get_value (getter_key);
	g_free (getter_key);

	/* If the values are the same, then we dont need to invoke any callbacks */
	if (nautilus_gconf_value_is_equal (entry->cached_value, new_value)) {
		nautilus_gconf_value_free (new_value);
		return;
	}

	/* Update the auto storage preferences */
	if (entry->auto_storage_list != NULL) {
		preferences_entry_update_auto_storage (entry);			
	}

	/* Store the new cached value */
	nautilus_gconf_value_free (entry->cached_value);
	entry->cached_value = new_value;
	
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

	g_return_if_fail (data != NULL);
	g_return_if_fail (callback_data != NULL);

	storage = (char **)data;
	value = (const char *)callback_data;

	g_free (*storage);
	*(char **)storage = g_strdup (value);
}

static void
update_auto_integer_or_boolean (gpointer data, gpointer callback_data)
{
	g_return_if_fail (data != NULL);

	*(int *)data = GPOINTER_TO_INT (callback_data);
}

static void
preferences_entry_update_auto_storage (PreferencesEntry *entry)
{
	char *new_string_value;
	int new_int_value;
	gboolean new_boolean_value;

	switch (entry->type) {
	case PREFERENCE_STRING:
		new_string_value = nautilus_preferences_get (entry->name);
		g_list_foreach (entry->auto_storage_list,
				update_auto_string,
				new_string_value);
		g_free (new_string_value);
		break;
	case PREFERENCE_INTEGER:
		new_int_value = nautilus_preferences_get_integer (entry->name);
		g_list_foreach (entry->auto_storage_list,
				update_auto_integer_or_boolean,
				GINT_TO_POINTER (new_int_value));
		break;
	case PREFERENCE_BOOLEAN:
		new_boolean_value = nautilus_preferences_get_boolean (entry->name);
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
	g_return_if_fail (entry != NULL);
	g_return_if_fail (entry->key != NULL);
	g_return_if_fail (notice_data != NULL);

	preferences_entry_invoke_callbacks_if_needed (notice_data);
}

static void
preferences_global_table_check_changes_function (gpointer key,
						 gpointer value,
						 gpointer user_data)
{
	PreferencesEntry *entry;

	g_return_if_fail (key != NULL);
	g_return_if_fail (value != NULL);

	entry = value;

	g_return_if_fail (entry->name != NULL);

	/* We dont worry about the 'user_level' itself for recursive reasons */
	if (preferences_preference_is_user_level (entry->name)) {
		return;
	}

	preferences_entry_invoke_callbacks_if_needed (entry);
}

static void
preferences_entry_update_cached_value (PreferencesEntry *entry)
{
	char *getter_key;

	g_return_if_fail (entry != NULL);

	nautilus_gconf_value_free (entry->cached_value);

	getter_key = preferences_key_make_for_getter (entry->name);
	entry->cached_value = nautilus_gconf_get_value (getter_key);
	g_free (getter_key);
}

static void
preferences_user_level_changed_notice (GConfClient *client, 
				       guint connection_id, 
				       GConfEntry *gconf_entry, 
				       gpointer user_data)
{
	g_return_if_fail (gconf_entry != NULL);
	g_return_if_fail (gconf_entry->key != NULL);
	g_return_if_fail (eel_str_has_suffix (gconf_entry->key, "user_level"));
	
	g_hash_table_foreach (preferences_global_table_get_global (),
			      preferences_global_table_check_changes_function,
			      NULL);
}

static void
preferences_entry_ensure_gconf_connection (PreferencesEntry *entry)
{
	GError *error;
	GConfClient *client;
	char *key;
	
	/*
	 * We install only one gconf notification for each preference entry.
	 * Otherwise, we would invoke the installed callbacks more than once
	 * per registered callback.
	 */
	if (entry->gconf_connection_id != 0) {
		return;
	}
		
	g_return_if_fail (entry->name != NULL);

	client = preferences_global_client_get ();

	g_return_if_fail (client != NULL);

	key = preferences_key_make (entry->name);

	error = NULL;
	entry->gconf_connection_id = gconf_client_notify_add (client,
							      key,
							      preferences_something_changed_notice,
							      entry,
							      NULL,
							      &error);
	if (nautilus_gconf_handle_error (&error)) {
		entry->gconf_connection_id = 0;
	}

	g_free (key);

	/* Update the cached value.
	 * From now onwards the cached value will be updated 
	 * each time preferences_something_changed_notice() triggers
	 * so that it can be later compared with new values to 
	 * determine if the gconf value is different from the 
	 * Nautilus value.
	 */
	preferences_entry_update_cached_value (entry);
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
				NautilusPreferencesCallback callback,
				gpointer callback_data)
{
	PreferencesCallbackEntry *callback_entry;

	g_return_if_fail (entry != NULL);
	g_return_if_fail (callback != NULL);

	callback_entry = g_new0 (PreferencesCallbackEntry, 1);
	callback_entry->callback = callback;
	callback_entry->callback_data = callback_data;
	
	g_return_if_fail (callback_entry != NULL);
	
	entry->callback_list = g_list_append (entry->callback_list, callback_entry);

	preferences_entry_ensure_gconf_connection (entry);
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
	g_return_if_fail (entry != NULL);
	g_return_if_fail (storage != NULL);
	g_return_if_fail (entry->type == 0 || entry->type == type);
	g_return_if_fail (g_list_find (entry->auto_storage_list, storage) == NULL);

	entry->type = type;
	
	entry->auto_storage_list = g_list_append (entry->auto_storage_list, storage);

	preferences_entry_ensure_gconf_connection (entry);
}

static void
preferences_entry_check_remove_connection (PreferencesEntry *entry)
{
	GConfClient *client;

	/*
	 * If there are no callbacks or auto-storage variables left in the entry, 
	 * remove the gconf notification.
	 */
	if (entry->callback_list != NULL || entry->auto_storage_list != NULL) {
		return;
	}

	client = preferences_global_client_get ();
	
	if (entry->gconf_connection_id != 0) {
		gconf_client_notify_remove (client, entry->gconf_connection_id);
	}
	
	entry->gconf_connection_id = 0;
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
				   NautilusPreferencesCallback callback,
				   gpointer callback_data)
{
	GList *new_list;
	GList *iterator;

	g_return_if_fail (entry != NULL);
	g_return_if_fail (callback != NULL);
	g_return_if_fail (entry->callback_list != NULL);
	
	new_list = g_list_copy (entry->callback_list);
	
	for (iterator = new_list; iterator != NULL; iterator = iterator->next) {
		PreferencesCallbackEntry *callback_entry = iterator->data;
		
		g_return_if_fail (callback_entry != NULL);
		
		if (callback_entry->callback == callback &&
		    callback_entry->callback_data == callback_data) {
			entry->callback_list = g_list_remove (entry->callback_list, 
							      callback_entry);
			
			preferences_callback_entry_free (callback_entry);
		}
	}

	g_list_free (new_list);

	preferences_entry_check_remove_connection (entry);
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
	GList *iterator;
	gpointer storage_in_entry;

	g_return_if_fail (entry != NULL);
	g_return_if_fail (storage != NULL);
	g_return_if_fail (entry->auto_storage_list != NULL);
	
	new_list = g_list_copy (entry->auto_storage_list);
	
	for (iterator = new_list; iterator != NULL; iterator = iterator->next) {
		storage_in_entry = iterator->data;
		
		g_return_if_fail (storage_in_entry != NULL);
		
		if (storage_in_entry == storage) {
			entry->auto_storage_list = g_list_remove (entry->auto_storage_list, 
							          storage);

			switch (entry->type) {
			case PREFERENCE_STRING:
				update_auto_string (storage, NULL);
				break;
			case PREFERENCE_BOOLEAN:
			case PREFERENCE_INTEGER:
				update_auto_integer_or_boolean (storage, 0);
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
	g_return_if_fail (callback_entry != NULL);

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
	g_return_if_fail (data != NULL);
	
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
	g_return_if_fail (entry != NULL);

	if (entry->gconf_connection_id != 0) {
		GConfClient *client;

		client = preferences_global_client_get ();
		g_assert (client != NULL);

		gconf_client_notify_remove (client, entry->gconf_connection_id);
		entry->gconf_connection_id = 0;
	}

	g_list_free (entry->auto_storage_list);
	eel_g_list_free_deep_custom (entry->callback_list,
					  preferences_callback_entry_free_func,
					  NULL);
	
	entry->auto_storage_list = NULL;
	entry->callback_list = NULL;

	g_free (entry->name);
	g_free (entry->description);
	g_free (entry->enumeration_id);

	nautilus_gconf_value_free (entry->cached_value);

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
}

static GHashTable *
preferences_global_table_get_global (void)
{
	if (global_table == NULL) {
		global_table = g_hash_table_new (g_str_hash, g_str_equal);
		g_atexit (preferences_global_table_free);
	}
	
	return global_table;
}

static PreferencesEntry *
preferences_global_table_lookup (const char *name)
{
	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (preferences_global_table_get_global () != NULL, NULL);
	
	return g_hash_table_lookup (preferences_global_table_get_global (), name);
}

static PreferencesEntry *
preferences_global_table_insert (const char *name)
{
	PreferencesEntry *entry;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (preferences_global_table_get_global () != NULL, NULL);
	g_return_val_if_fail (preferences_global_table_lookup (name) == NULL, NULL);
	
	entry = g_new0 (PreferencesEntry, 1);
	entry->name = g_strdup (name);

	g_hash_table_insert (preferences_global_table_get_global (), entry->name, entry);

	g_return_val_if_fail (entry == preferences_global_table_lookup (name), NULL);

	/* Update the cached value for the first time.
	 * 
	 * We need to do this because checks for value changes
	 * happen not only as a result of callbacks triggering, but 
	 * also as a result of user_level changes.  When a user level
	 * changes, all the preferences entries are iterated to invoke
	 * callbacks for those that changed as a result.
	 *
	 * See preferences_global_table_check_changes_function().
	 */
	preferences_entry_update_cached_value (entry);

	return entry;
}

static PreferencesEntry *
preferences_global_table_lookup_or_insert (const char *name)
{
	PreferencesEntry *entry;

	g_return_val_if_fail (name != NULL, NULL);
	
	entry = preferences_global_table_lookup (name);

	if (entry != NULL) {
		return entry;
	}

	entry = preferences_global_table_insert (name);
	g_assert (entry != NULL);

	return entry;
}

void
nautilus_preferences_add_callback (const char *name,
				   NautilusPreferencesCallback callback,
				   gpointer callback_data)
{
	PreferencesEntry *entry;

	g_return_if_fail (name != NULL);
	g_return_if_fail (callback != NULL);

	entry = preferences_global_table_lookup_or_insert (name);
	g_assert (entry != NULL);

	preferences_entry_add_callback (entry, callback, callback_data);
}

void
nautilus_preferences_add_auto_string (const char *name,
				      const char **storage)
{
	PreferencesEntry *entry;
	char *value;

	g_return_if_fail (name != NULL);
	g_return_if_fail (storage != NULL);

	entry = preferences_global_table_lookup_or_insert (name);
	g_assert (entry != NULL);

	preferences_entry_add_auto_storage (entry, storage, PREFERENCE_STRING);

	value = nautilus_preferences_get (entry->name);
	update_auto_string (storage, value);
	g_free (value);
}

void
nautilus_preferences_add_auto_integer (const char *name,
				       int *storage)
{
	PreferencesEntry *entry;
	int value;

	g_return_if_fail (name != NULL);
	g_return_if_fail (storage != NULL);

	entry = preferences_global_table_lookup_or_insert (name);
	g_assert (entry != NULL);

	preferences_entry_add_auto_storage (entry, storage, PREFERENCE_INTEGER);

	value = nautilus_preferences_get_integer (entry->name);
	update_auto_integer_or_boolean (storage, GINT_TO_POINTER (value));
}

void
nautilus_preferences_add_auto_boolean (const char *name,
				       gboolean *storage)
{
	PreferencesEntry *entry;
	gboolean value;

	g_return_if_fail (name != NULL);
	g_return_if_fail (storage != NULL);

	entry = preferences_global_table_lookup_or_insert (name);
	g_assert (entry != NULL);

	preferences_entry_add_auto_storage (entry, storage, PREFERENCE_BOOLEAN);

	value = nautilus_preferences_get_boolean (entry->name);
	update_auto_integer_or_boolean (storage, GINT_TO_POINTER (value));
}

void
nautilus_preferences_remove_auto_string (const char *name,
				         const char **storage)
{
	PreferencesEntry *entry;

	g_return_if_fail (name != NULL);
	g_return_if_fail (storage != NULL);

	entry = preferences_global_table_lookup (name);
	if (entry == NULL) {
		g_warning ("Trying to remove auto-string for %s without adding it first.", name);
		return;
	}

	preferences_entry_remove_auto_storage (entry, storage);
}

void
nautilus_preferences_remove_auto_integer (const char *name,
				          int *storage)
{
	PreferencesEntry *entry;

	g_return_if_fail (name != NULL);
	g_return_if_fail (storage != NULL);

	entry = preferences_global_table_lookup (name);
	if (entry == NULL) {
		g_warning ("Trying to remove auto-integer for %s without adding it first.", name);
		return;
	}

	preferences_entry_remove_auto_storage (entry, storage);
}

void
nautilus_preferences_remove_auto_boolean (const char *name,
				          gboolean *storage)
{
	PreferencesEntry *entry;

	g_return_if_fail (name != NULL);
	g_return_if_fail (storage != NULL);

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
	NautilusPreferencesCallback callback;
	gpointer callback_data;
} WhileAliveData;

static void
preferences_while_alive_disconnector (GtkObject *object, gpointer callback_data)
{
	WhileAliveData *data;

	g_return_if_fail (GTK_IS_OBJECT (object));
	g_return_if_fail (callback_data != NULL);

	data = callback_data;

	nautilus_preferences_remove_callback (data->name,
					      data->callback,
					      data->callback_data);

	g_free (data->name);
	g_free (data);
}

void
nautilus_preferences_add_callback_while_alive (const char *name,
					       NautilusPreferencesCallback callback,
					       gpointer callback_data,
					       GtkObject *alive_object)
{
	WhileAliveData *data;

	g_return_if_fail (name != NULL);
	g_return_if_fail (callback != NULL);
	g_return_if_fail (GTK_IS_OBJECT (alive_object));

	data = g_new (WhileAliveData, 1);
	data->name = g_strdup (name);
	data->callback = callback;
	data->callback_data = callback_data;

	nautilus_preferences_add_callback (name, callback, callback_data);

	gtk_signal_connect (alive_object,
			    "destroy",
			    GTK_SIGNAL_FUNC (preferences_while_alive_disconnector),
			    data);
}

void
nautilus_preferences_remove_callback (const char *name,
				      NautilusPreferencesCallback callback,
				      gpointer callback_data)
{
	PreferencesEntry *entry;

	g_return_if_fail (name != NULL);
	g_return_if_fail (callback != NULL);

	entry = preferences_global_table_lookup (name);

	if (entry == NULL) {
		g_warning ("Trying to remove a callback for %s without adding it first.", name);
		return;
	}
	
	preferences_entry_remove_callback (entry, callback, callback_data);
}

void
nautilus_preferences_set_description (const char *name,
				      const char *description)
{
	PreferencesEntry *entry;

	g_return_if_fail (name != NULL);
	g_return_if_fail (description != NULL);

	entry = preferences_global_table_lookup_or_insert (name);
	g_assert (entry != NULL);

	g_free (entry->description);
	entry->description = g_strdup (description);
}

char *
nautilus_preferences_get_description (const char *name)
{
	PreferencesEntry *entry;

	g_return_val_if_fail (name != NULL, NULL);

	entry = preferences_global_table_lookup_or_insert (name);

	return g_strdup (entry->description ? entry->description : "");
}

void
nautilus_preferences_set_enumeration_id (const char *name,
					 const char *enumeration_id)
{
	PreferencesEntry *entry;

	g_return_if_fail (name != NULL);
	g_return_if_fail (enumeration_id != NULL);

	entry = preferences_global_table_lookup_or_insert (name);
	g_assert (entry != NULL);

	g_free (entry->enumeration_id);
	entry->enumeration_id = g_strdup (enumeration_id);
}

char *
nautilus_preferences_get_enumeration_id (const char *name)
{
	PreferencesEntry *entry;

	g_return_val_if_fail (name != NULL, NULL);

	entry = preferences_global_table_lookup_or_insert (name);

	return entry->enumeration_id ? g_strdup (entry->enumeration_id) : NULL;
}

char *
nautilus_preferences_get_user_level_name_for_display (int user_level)
{
	user_level = preferences_user_level_check_range (user_level);
	
	return g_strdup (gettext (user_level_names_for_display[user_level]));
}

char *
nautilus_preferences_get_user_level_name_for_storage (int user_level)
{
	user_level = preferences_user_level_check_range (user_level);
	
	return g_strdup (user_level_names_for_storage[user_level]);
}

static int
preferences_user_level_check_range (int user_level)
{
	user_level = MAX (user_level, 0);
	user_level = MIN (user_level, 2);

	return user_level;
}

gboolean
nautilus_preferences_monitor_directory (const char *directory)
{
	return nautilus_gconf_monitor_directory (directory);
}

gboolean
nautilus_preferences_is_visible (const char *name)
{
	int user_level;
	int visible_user_level;

	g_return_val_if_fail (name != NULL, FALSE);

	user_level = nautilus_preferences_get_user_level ();
	visible_user_level = nautilus_preferences_get_visible_user_level (name);

	return visible_user_level <= user_level;
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)
void
nautilus_self_check_preferences (void)
{
}
#endif /* !NAUTILUS_OMIT_SELF_CHECK */
