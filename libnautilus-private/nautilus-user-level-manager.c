/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-user-level-manager.c - User level manager class.
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "nautilus-user-level-manager.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-glib-extensions.h"

#include <gtk/gtksignal.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#define NAUTILUS_TYPE_USER_LEVEL_MANAGER            (nautilus_user_level_manager_get_type ())
#define NAUTILUS_USER_LEVEL_MANAGER(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_USER_LEVEL_MANAGER, NautilusUserLevelManager))
#define NAUTILUS_USER_LEVEL_MANAGER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_USER_LEVEL_MANAGER, NautilusUserLevelManagerClass))
#define NAUTILUS_IS_USER_LEVEL_MANAGER(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_USER_LEVEL_MANAGER))
#define NAUTILUS_IS_USER_LEVEL_MANAGER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_USER_LEVEL_MANAGER))

static const char *DEFAULT_USER_LEVEL_NAMES[] =
{
	"novice",
	"intermediate",
	"hacker"
};

#define USER_LEVEL_NOVICE	0
#define USER_LEVEL_INTERMEDIATE	1
#define USER_LEVEL_HACKER	2

static const guint   DEFAULT_NUM_USER_LEVELS = NAUTILUS_N_ELEMENTS (DEFAULT_USER_LEVEL_NAMES);
static const guint   DEFAULT_USER_LEVEL = USER_LEVEL_HACKER;

static const char    USER_LEVEL_KEY[] = "/apps/nautilus/user_level";
static const char    USER_LEVEL_PATH[] = "/apps/nautilus";

static NautilusUserLevelManager *global_manager = NULL;

struct _NautilusUserLevelManager 
{
	GtkObject		object;

        guint			num_user_levels;
	NautilusStringList	*user_level_names;

	GConfClient		*gconf_client;
	int			user_level_changed_connection;
};

typedef struct 
{
	GtkObjectClass	parent_class;
} NautilusUserLevelManagerClass;

enum
{
	USER_LEVEL_CHANGED,
	LAST_SIGNAL
};

static guint user_level_manager_signals[LAST_SIGNAL];

static GtkType                   nautilus_user_level_manager_get_type         (void);
static void                      nautilus_user_level_manager_initialize_class (NautilusUserLevelManagerClass *user_level_manager_class);
static void                      nautilus_user_level_manager_initialize       (NautilusUserLevelManager      *user_level_manager);
static void                      user_level_manager_destroy                   (GtkObject                     *object);
static NautilusUserLevelManager *user_level_manager_new                       (void);
static void                      user_level_manager_ensure_global_manager     (void);
static void                      user_level_set_default_if_needed             (NautilusUserLevelManager      *manager);


/* Gconf callbacks */
static void                      gconf_user_level_changed_callback            (GConfClient                   *client,
									       guint                          cnxn_id,
									       const gchar                   *key,
									       GConfValue                    *value,
									       gboolean                       is_default,
									       gpointer                       user_data);


NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusUserLevelManager, nautilus_user_level_manager, GTK_TYPE_OBJECT)

/* Create the icon factory. */
static NautilusUserLevelManager *
user_level_manager_new (void)
{
        NautilusUserLevelManager *manager;
        guint			 i;

	if (!gconf_is_initialized ()) {
		GConfError	  *error = NULL;
		char		  *argv[] = { "nautilus", NULL };
		
		if (!gconf_init (1, argv, &error)) {
			g_assert (error != NULL);
			
			/* FIXME bugzilla.eazel.com 672: Need better error reporting here */
			g_warning ("GConf init failed:\n  %s", error->str);
			
			gconf_error_destroy (error);
			
			error = NULL;
			
			return NULL;
		}
	}

        manager = NAUTILUS_USER_LEVEL_MANAGER (gtk_object_new (nautilus_user_level_manager_get_type (), NULL));

	manager->gconf_client = gconf_client_get_default ();

	/* Let gconf know about ~/.gconf/apps/nautilus */
	gconf_client_add_dir (manager->gconf_client,
			      USER_LEVEL_PATH,
			      GCONF_CLIENT_PRELOAD_RECURSIVE,
			      NULL);
	
	manager->num_user_levels = DEFAULT_NUM_USER_LEVELS;
	manager->user_level_names = nautilus_string_list_new ();

	for (i = 0; i < DEFAULT_NUM_USER_LEVELS; i++) {
		nautilus_string_list_insert (manager->user_level_names, DEFAULT_USER_LEVEL_NAMES[i]);
	}
	
	g_assert (manager->gconf_client != NULL);

	user_level_set_default_if_needed (manager);

	/* Add a gconf notification for user_level changes.  */
	manager->user_level_changed_connection = gconf_client_notify_add (manager->gconf_client,
									  USER_LEVEL_KEY,
									  gconf_user_level_changed_callback,
									  NULL,
									  NULL,
									  NULL);
	
        return manager;
}

static void
nautilus_user_level_manager_initialize (NautilusUserLevelManager *manager)
{
	manager->num_user_levels = 0;

	manager->gconf_client = NULL;
	manager->user_level_changed_connection = 0;
}

static void
nautilus_user_level_manager_initialize_class (NautilusUserLevelManagerClass *user_level_manager_class)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (user_level_manager_class);
	
	user_level_manager_signals[USER_LEVEL_CHANGED] = gtk_signal_new ("user_level_changed",
									 GTK_RUN_LAST,
									 object_class->type,
									 0,
									 gtk_marshal_NONE__NONE,
									 GTK_TYPE_NONE,
									 0);
	
	gtk_object_class_add_signals (object_class, user_level_manager_signals, LAST_SIGNAL);

	/* GtkObjectClass */
	object_class->destroy = user_level_manager_destroy;
}

static void
user_level_manager_destroy (GtkObject *object)
{
	NautilusUserLevelManager *manager;
	
	manager = NAUTILUS_USER_LEVEL_MANAGER (object);

	/* Right now, the global manager is not destroyed on purpose */
	g_assert_not_reached ();

	/* Remove the gconf notification if its still lingering */
	if (manager->user_level_changed_connection != 0) {
		gconf_client_notify_remove (manager->gconf_client,
					    manager->user_level_changed_connection);
		
		manager->user_level_changed_connection = 0;
	}

	if (manager->gconf_client != NULL) {
		gtk_object_unref (GTK_OBJECT (manager->gconf_client));
	}

	nautilus_string_list_free (manager->user_level_names);
}

static void
user_level_manager_ensure_global_manager (void)
{
        if (global_manager == NULL) {
                global_manager = user_level_manager_new ();
        }

	g_assert (global_manager != NULL);
}

static void
user_level_set_default_if_needed (NautilusUserLevelManager *manager)
{
	GConfValue *value;

	g_assert (manager != NULL);
	g_assert (manager->gconf_client != NULL);
	
	value = gconf_client_get_without_default (manager->gconf_client, USER_LEVEL_KEY, NULL);
	
	if (!value) {
		value = gconf_value_new (GCONF_VALUE_STRING);
		
		gconf_value_set_string (value, DEFAULT_USER_LEVEL_NAMES[DEFAULT_USER_LEVEL]);
		
		gconf_client_set (manager->gconf_client, USER_LEVEL_KEY, value, NULL);

		gconf_client_suggest_sync (manager->gconf_client, NULL);
	}

	g_assert (value != NULL);

	gconf_value_destroy (value);
}

static void
gconf_user_level_changed_callback (GConfClient	*client, 
				   guint	connection_id, 
				   const gchar	*key, 
				   GConfValue	*value, 
				   gboolean	is_default, 
				   gpointer	user_data)
{
	NautilusUserLevelManager *manager = nautilus_user_level_manager_get ();

	gtk_signal_emit (GTK_OBJECT (manager), user_level_manager_signals[USER_LEVEL_CHANGED]);
}

/* Public NautilusUserLevelManager functions */
NautilusUserLevelManager *
nautilus_user_level_manager_get (void)
{
	user_level_manager_ensure_global_manager ();

	g_assert (global_manager != NULL);
	g_assert (NAUTILUS_IS_USER_LEVEL_MANAGER (global_manager));
	
        return global_manager;
}

void
nautilus_user_level_manager_set_user_level (guint user_level)
{
	NautilusUserLevelManager *manager = nautilus_user_level_manager_get ();
	char			 *user_level_string;
	gboolean		 result;
	guint			 old_user_level;

	g_return_if_fail (user_level < manager->num_user_levels);
	g_return_if_fail (user_level < nautilus_string_list_get_length (manager->user_level_names));

	old_user_level = nautilus_user_level_manager_get_user_level ();

	if (old_user_level == user_level) {
		return;
	}

	user_level_string = nautilus_string_list_nth (manager->user_level_names, user_level);
	
	g_assert (user_level_string != NULL);

	result = gconf_client_set_string (manager->gconf_client,
					  USER_LEVEL_KEY,
					  user_level_string,
					  NULL);

	g_assert (result);

	gconf_client_suggest_sync (manager->gconf_client, NULL);
}

guint
nautilus_user_level_manager_get_user_level (void)
{
	NautilusUserLevelManager *manager = nautilus_user_level_manager_get ();
	char			 *user_level_string;
	gint			 index;

	user_level_string = nautilus_user_level_manager_get_user_level_as_string ();
	/* FIXME bugzilla.eazel.com 1293: 
	 * Asserting based on something that's read from GConf
	 * seems like a bad idea. It means we core dump if
	 * something's wrong.
	 */
	g_assert (user_level_string != NULL);

	index = nautilus_string_list_get_index_for_string (manager->user_level_names,
							   user_level_string);

	g_free (user_level_string);

	/* FIXME bugzilla.eazel.com 1293: 
	 * Asserting based on something that's read from GConf
	 * seems like a bad idea. It means we core dump if
	 * something's wrong.
	 */
	g_assert (index != NAUTILUS_STRING_LIST_NOT_FOUND);

	return (guint) index;
}

guint
nautilus_user_level_manager_get_num_user_levels (void)
{
	NautilusUserLevelManager *manager = nautilus_user_level_manager_get ();

	return manager->num_user_levels;
}

NautilusStringList *
nautilus_user_level_manager_get_user_level_names (void)
{
	NautilusUserLevelManager *manager = nautilus_user_level_manager_get ();

	return nautilus_string_list_new_from_string_list (manager->user_level_names);
}

char *
nautilus_user_level_manager_make_gconf_key (const char *preference_name,
					    guint user_level)
{
	NautilusUserLevelManager *manager = nautilus_user_level_manager_get ();

	char *key;
	char *user_level_string;

	g_return_val_if_fail (preference_name != NULL, NULL);
	g_return_val_if_fail (user_level < manager->num_user_levels, NULL);
	g_return_val_if_fail (user_level < nautilus_string_list_get_length (manager->user_level_names), NULL);
	
	user_level_string = nautilus_string_list_nth (manager->user_level_names, user_level);
	g_assert (user_level_string != NULL);

	key = g_strdup_printf ("%s/%s/%s",
			       USER_LEVEL_PATH,
			       user_level_string,
			       preference_name);

	g_free (user_level_string);

	return key;
}

char *
nautilus_user_level_manager_make_current_gconf_key (const char *preference_name)
{
	return nautilus_user_level_manager_make_gconf_key (preference_name,
							   nautilus_user_level_manager_get_user_level ());
}

char *
nautilus_user_level_manager_get_user_level_as_string (void)
{
	NautilusUserLevelManager *manager = nautilus_user_level_manager_get ();
	char			 *user_level_string;

	g_assert (manager->gconf_client != NULL);
	
	user_level_string = gconf_client_get_string (manager->gconf_client, USER_LEVEL_KEY, NULL);

	if (!user_level_string)
		user_level_string = g_strdup ("novice");

	return user_level_string;
}


/**
 * nautilus_user_level_manager_set_default_value_if_needed
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
void
nautilus_user_level_manager_set_default_value_if_needed (const char		*preference_name,
							 NautilusPreferenceType       type,
							 guint			user_level,
							 gconstpointer		default_value)
{
	NautilusUserLevelManager *manager = nautilus_user_level_manager_get ();
	GConfValue	*value = NULL;
	char		*key;

	g_return_if_fail (preference_name != NULL);

	key = nautilus_user_level_manager_make_gconf_key (preference_name, user_level);
	g_assert (key != NULL);

	/* Find out if the preference exists at all */
	value = gconf_client_get_without_default (manager->gconf_client, key, NULL);
	
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
			gconf_client_set (manager->gconf_client, key, value, NULL);
		}
	}
	
	if (value) {
		gconf_value_destroy (value);
	}

	g_free (key);
}

gboolean
nautilus_user_level_manager_compare_preference_between_user_levels (const char *preference_name,
								    guint	 user_level_a,
								    guint	 user_level_b)
{
	NautilusUserLevelManager *manager = nautilus_user_level_manager_get ();
	gboolean	result = FALSE;
	char		*key_a;
	char		*key_b;
	GConfValue	*value_a;
	GConfValue	*value_b;

	g_return_val_if_fail (preference_name != NULL, FALSE);

	key_a = nautilus_user_level_manager_make_gconf_key (preference_name, user_level_a);
	g_assert (key_a != NULL);

	key_b = nautilus_user_level_manager_make_gconf_key (preference_name, user_level_b);
	g_assert (key_b != NULL);

	value_a = gconf_client_get (manager->gconf_client, key_a, NULL);
	value_b = gconf_client_get (manager->gconf_client, key_b, NULL);

	g_free (key_a);
	g_free (key_b);

	if (value_a && value_b)
	{
		g_assert (value_a->type == value_b->type);
		
		switch (value_a->type)
		{
		case GCONF_VALUE_STRING:
			result = (gconf_value_string (value_a)
				  && gconf_value_string (value_b)
				  && (strcmp (gconf_value_string (value_a), gconf_value_string (value_b)) == 0));
			break;
			
		case GCONF_VALUE_INT:
			result = (gconf_value_int (value_a) == gconf_value_int (value_b));
			break;
			
		case GCONF_VALUE_BOOL:
			result = (gconf_value_bool (value_a) == gconf_value_bool (value_b));
			break;

		default:
			g_assert_not_reached ();
		}
	}
	else
	{
		result = TRUE;
	}

	if (value_a) {
		gconf_value_destroy (value_a);
	}

	if (value_b) {
		gconf_value_destroy (value_b);
	}

	return result;
}
