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
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>

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

static const char    USER_LEVEL_KEY[] = "/nautilus/user_level";

struct _NautilusUserLevelManager 
{
	GtkObject		object;

        guint			num_user_levels;
	NautilusStringList	*user_level_names;

	GConfClient		*gconf_client;
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
static gboolean                  user_level_manager_is_initialized            (void);
static NautilusUserLevelManager *user_level_manager_get_global_manager        (void);
static void                      user_level_set_default_if_needed             (void);
char *                           get_gconf_user_level_string                  (void);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusUserLevelManager, nautilus_user_level_manager, GTK_TYPE_OBJECT)

/* Create the icon factory. */
static NautilusUserLevelManager *
user_level_manager_new ()
{
        NautilusUserLevelManager *manager;
        guint			 i;

        manager = NAUTILUS_USER_LEVEL_MANAGER (gtk_object_new (nautilus_user_level_manager_get_type (), NULL));

	manager->gconf_client = gconf_client_new ();
	manager->num_user_levels = DEFAULT_NUM_USER_LEVELS;
	manager->user_level_names = nautilus_string_list_new ();

	for (i = 0; i < DEFAULT_NUM_USER_LEVELS; i++) {
		nautilus_string_list_insert (manager->user_level_names, DEFAULT_USER_LEVEL_NAMES[i]);
	}
	
	g_assert (manager->gconf_client != NULL);

	user_level_set_default_if_needed ();

	g_print ("user_level_manager_new: user_level = %d\n", nautilus_user_level_manager_get_user_level ());

        return manager;
}

static void
nautilus_user_level_manager_initialize (NautilusUserLevelManager *manager)
{
	manager->num_user_levels = 0;

	manager->gconf_client = NULL;
}

static void
nautilus_user_level_manager_initialize_class (NautilusUserLevelManagerClass *user_level_manager_class)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (user_level_manager_class);
	
	user_level_manager_signals[USER_LEVEL_CHANGED] = gtk_signal_new ("icons_changed",
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

	if (manager->gconf_client != NULL) {
		gtk_object_unref (GTK_OBJECT (manager->gconf_client));
	}

	nautilus_string_list_free (manager->user_level_names);
}

static NautilusUserLevelManager *
user_level_manager_get_global_manager (void)
{
        static NautilusUserLevelManager *global_manager = NULL;
	
        if (global_manager == NULL) {
                global_manager = user_level_manager_new ();
        }

        return global_manager;
}

static gboolean
user_level_manager_is_initialized (void)
{
        NautilusUserLevelManager *manager = user_level_manager_get_global_manager ();
	
        return (manager && manager->num_user_levels && manager->user_level_names);
}

char *
get_gconf_user_level_string (void)
{
	NautilusUserLevelManager *manager = nautilus_user_level_manager_get ();
	char			 *user_level_string;

	g_assert (manager->gconf_client != NULL);
	
	user_level_string = gconf_client_get_string (manager->gconf_client, USER_LEVEL_KEY, NULL);

	return user_level_string;
}

static void
user_level_set_default_if_needed (void)
{
        NautilusUserLevelManager *manager = user_level_manager_get_global_manager ();
	GConfValue		 *value;

	g_assert (manager->gconf_client != NULL);
	
	value = gconf_client_get_without_default (manager->gconf_client, USER_LEVEL_KEY, NULL);
	
	if (!value) {
		value = gconf_value_new (GCONF_VALUE_STRING);
		
		gconf_value_set_string (value, DEFAULT_USER_LEVEL_NAMES[DEFAULT_USER_LEVEL]);
		
		gconf_client_set (manager->gconf_client, USER_LEVEL_KEY, value, NULL);

		gconf_client_suggest_sync (manager->gconf_client, NULL);

		g_print ("setting default user_level to %s\n", DEFAULT_USER_LEVEL_NAMES[DEFAULT_USER_LEVEL]);
	}

	g_assert (value != NULL);

	gconf_value_destroy (value);
}

/* Public NautilusUserLevelManager functions */
NautilusUserLevelManager *
nautilus_user_level_manager_get (void)
{
        NautilusUserLevelManager *manager = user_level_manager_get_global_manager ();
	
	g_assert (user_level_manager_is_initialized ());

        return manager;
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
	
	g_print ("user_level changed from %d to %d\n", old_user_level, user_level);

	gtk_signal_emit (GTK_OBJECT (manager), user_level_manager_signals[USER_LEVEL_CHANGED]);
}

guint
nautilus_user_level_manager_get_user_level (void)
{
	NautilusUserLevelManager *manager = nautilus_user_level_manager_get ();
	char			 *user_level_string;
	gint			 index;

	user_level_string = get_gconf_user_level_string ();
	g_assert (user_level_string != NULL);

	index = nautilus_string_list_get_index_for_string (manager->user_level_names,
							   user_level_string);

	g_free (user_level_string);

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

