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
#include <libnautilus/nautilus-gtk-macros.h>

static const char PREFERENCES_GLOBAL_DOMAIN[] = "Nautilus::Global";

enum {
	ACTIVATE,
	LAST_SIGNAL
};

typedef struct {
	NautilusPreferencesInfo	pref_info;
	gpointer		pref_value;
	GList			*callback_list;
} PrefHashInfo;

typedef struct {
	NautilusPreferencesCallback	callback_proc;
	gpointer		user_data;
	const PrefHashInfo	*hash_info;
} PrefCallbackInfo;

struct NautilusPreferencesDetails {
	char		*domain;
	GHashTable	*prefs_hash_table;
};

/* NautilusPreferencesClass methods */
static void              nautilus_preferences_initialize_class (NautilusPreferencesClass      *klass);
static void              nautilus_preferences_initialize       (NautilusPreferences           *prefs);

/* GtkObjectClass methods */
static void              prefs_destroy                         (GtkObject                     *object);

/* PrefHashInfo functions */
static PrefHashInfo *    pref_hash_info_alloc                  (const NautilusPreferencesInfo *pref_info);
static void              pref_hash_info_free                   (PrefHashInfo                  *pref_hash_info);
static void              pref_hash_info_free_func              (gpointer                       key,
								gpointer                       value,
								gpointer                       user_data);

/* PrefCallbackInfo functions */
static PrefCallbackInfo *pref_callback_info_alloc              (NautilusPreferencesCallback    callback_proc,
								gpointer                       user_data,
								const PrefHashInfo            *hash_info);
static void              pref_callback_info_free               (PrefCallbackInfo              *pref_hash_info);
static void              pref_callback_info_free_func          (gpointer                       data,
								gpointer                       user_data);
static void              pref_callback_info_invoke_func        (gpointer                       data,
								gpointer                       user_data);
static void              pref_hash_info_add_callback           (PrefHashInfo                  *pref_hash_info,
								NautilusPreferencesCallback    callback_proc,
								gpointer                       user_data);
static void              pref_hash_info_remove_callback        (PrefHashInfo                  *pref_hash_info,
								NautilusPreferencesCallback    callback_proc,
								gpointer                       user_data);

/* Private stuff */
static gboolean          prefs_check_supported_type            (NautilusPreferencesType        type);
static void              prefs_set_pref                        (NautilusPreferences           *prefs,
								const char                    *name,
								gconstpointer                  value);
static gboolean          prefs_get_pref                        (NautilusPreferences           *prefs,
								const char                    *name,
								NautilusPreferencesType       *type_out,
								gconstpointer                 *pref_value_out);
PrefHashInfo *           prefs_hash_lookup                     (NautilusPreferences           *prefs,
								const char                    *name);
static char *            make_gnome_config_string              (const NautilusPreferencesInfo *pref_info);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusPreferences, nautilus_preferences, GTK_TYPE_OBJECT)

/*
 * NautilusPreferencesClass methods
 */
static void
nautilus_preferences_initialize_class (NautilusPreferencesClass *prefs_class)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (prefs_class);

 	parent_class = gtk_type_class (gtk_object_get_type ());

	/* GtkObjectClass */
	object_class->destroy = prefs_destroy;
}

static void
nautilus_preferences_initialize (NautilusPreferences *prefs)
{
	prefs->details = g_new (NautilusPreferencesDetails, 1);

	prefs->details->domain = NULL;

	prefs->details->prefs_hash_table = g_hash_table_new (g_str_hash, 
							     g_str_equal);
}

/*
 * GtkObjectClass methods
 */
static void
prefs_destroy (GtkObject *object)
{
	NautilusPreferences * prefs;
	
	prefs = NAUTILUS_PREFERENCES (object);

	g_free (prefs->details->domain);

	if (prefs->details->prefs_hash_table != NULL) {
		g_hash_table_foreach (prefs->details->prefs_hash_table,
				      pref_hash_info_free_func,
				      NULL);
		
		g_hash_table_destroy (prefs->details->prefs_hash_table);

		prefs->details->prefs_hash_table = NULL;
	}

	g_free (prefs->details);
	
	/* Chain */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/*
 * PrefHashInfo functions
 */
static PrefHashInfo *
pref_hash_info_alloc (const NautilusPreferencesInfo *pref_info)
{
	PrefHashInfo * pref_hash_info;
	
	g_assert (pref_info != NULL);

	g_assert (pref_info->name != NULL);
	g_assert (pref_info->description != NULL);

	pref_hash_info = g_new (PrefHashInfo, 1);

	pref_hash_info->pref_info.name = g_strdup (pref_info->name);
	pref_hash_info->pref_info.description = g_strdup (pref_info->description);
	pref_hash_info->pref_info.type = pref_info->type;
	pref_hash_info->pref_info.default_value = pref_info->default_value;
	pref_hash_info->pref_info.data = pref_info->data;

	pref_hash_info->pref_value = (gpointer) pref_info->default_value;
	pref_hash_info->callback_list = NULL;

	return pref_hash_info;
}

static void
pref_hash_info_free (PrefHashInfo *pref_hash_info)
{
	g_assert (pref_hash_info != NULL);

	g_assert (pref_hash_info->pref_info.name != NULL);
	g_assert (pref_hash_info->pref_info.description != NULL);

	g_list_foreach (pref_hash_info->callback_list,
			pref_callback_info_free_func,
			NULL);
	
	g_free (pref_hash_info->pref_info.name);
	g_free (pref_hash_info->pref_info.description);

	pref_hash_info->pref_info.name = NULL;
	pref_hash_info->pref_info.type = GTK_TYPE_INVALID;
	pref_hash_info->pref_info.default_value = NULL;
	pref_hash_info->pref_info.data = NULL;

	pref_hash_info->callback_list = NULL;
	pref_hash_info->pref_value = NULL;

	g_free (pref_hash_info);
}

static void
pref_hash_info_add_callback (PrefHashInfo		*pref_hash_info,
			     NautilusPreferencesCallback	callback_proc,
			     gpointer			user_data)
{
	PrefCallbackInfo * pref_callback_info;

	g_assert (pref_hash_info != NULL);

	g_assert (callback_proc != NULL);

	pref_callback_info = pref_callback_info_alloc (callback_proc, 
						       user_data,
						       pref_hash_info);

	g_assert (pref_callback_info != NULL);
	
	pref_hash_info->callback_list =
		g_list_append (pref_hash_info->callback_list, 
			       (gpointer) pref_callback_info);
}

static void
pref_hash_info_remove_callback (PrefHashInfo			*pref_hash_info,
				NautilusPreferencesCallback	callback_proc,
				gpointer			user_data)
{
	GList		 *new_list;
	GList		 *iterator;

	g_assert (pref_hash_info != NULL);

	g_assert (callback_proc != NULL);

	g_assert (pref_hash_info->callback_list != NULL);

	new_list = g_list_copy (pref_hash_info->callback_list);

	for (iterator = new_list; iterator != NULL; iterator = iterator->next) {
		PrefCallbackInfo *callback_info = (PrefCallbackInfo *) iterator->data;

		g_assert (callback_info != NULL);

		if (callback_info->callback_proc == callback_proc &&
		    callback_info->user_data == user_data) {

			pref_hash_info->callback_list = 
				g_list_remove (pref_hash_info->callback_list, 
					       (gpointer) callback_info);
			
			pref_callback_info_free (callback_info);
		}
	}
}

static void
pref_hash_info_free_func (gpointer key,
			  gpointer value,
			  gpointer user_data)
{
	PrefHashInfo *pref_hash_info;

	pref_hash_info = (PrefHashInfo *) value;

	g_assert (pref_hash_info != NULL);

	pref_hash_info_free (pref_hash_info);
}

/*
 * PrefCallbackInfo functions
 */
static PrefCallbackInfo *
pref_callback_info_alloc (NautilusPreferencesCallback	callback_proc,
			  gpointer		user_data,
			  const PrefHashInfo	*hash_info)
{
	PrefCallbackInfo * pref_callback_info;
	
	g_assert (callback_proc != NULL);

	pref_callback_info = g_new (PrefCallbackInfo, 1);

	pref_callback_info->callback_proc = callback_proc;
	pref_callback_info->user_data = user_data;
	pref_callback_info->hash_info = hash_info;

	return pref_callback_info;
}

static void
pref_callback_info_free (PrefCallbackInfo *pref_callback_info)
{
	g_assert (pref_callback_info != NULL);

	pref_callback_info->callback_proc = NULL;
	pref_callback_info->user_data = NULL;

	g_free (pref_callback_info);
}

static void
pref_callback_info_free_func (gpointer	data,
			      gpointer	user_data)
{
	PrefCallbackInfo *pref_callback_info;

	pref_callback_info = (PrefCallbackInfo *) data;

	g_assert (pref_callback_info != NULL);

	pref_callback_info_free (pref_callback_info);
}

static void
pref_callback_info_invoke_func (gpointer	data,
				gpointer	user_data)
{
 	NautilusPreferences     *prefs;
	PrefCallbackInfo *pref_callback_info;

	pref_callback_info = (PrefCallbackInfo *) data;

	g_assert (pref_callback_info != NULL);

	g_assert (pref_callback_info->callback_proc != NULL);

 	prefs = (NautilusPreferences *) user_data;

 	(* pref_callback_info->callback_proc) (prefs,
					       pref_callback_info->hash_info->pref_info.name,
					       pref_callback_info->hash_info->pref_info.type,
					       pref_callback_info->hash_info->pref_value,
					       pref_callback_info->user_data);
}

/*
 * Private stuff
 */
static gboolean
prefs_check_supported_type (NautilusPreferencesType type)
{
	return type == NAUTILUS_PREFERENCE_BOOLEAN
		|| type == NAUTILUS_PREFERENCE_ENUM
		|| type == NAUTILUS_PREFERENCE_STRING;
}

static void
prefs_set_pref (NautilusPreferences *prefs,
		const char *name,
		gconstpointer pref_value)
{
	PrefHashInfo * pref_hash_info;

	g_assert (NAUTILUS_IS_PREFERENCES (prefs));
	g_assert (name != NULL);

	pref_hash_info = prefs_hash_lookup (prefs, name);
	if (!pref_hash_info) {
		g_warning ("tried to set an unregistered preference '%s'", name);
		return;
	}

	/* gnome-config for now ; in the future gconf */
	switch (pref_hash_info->pref_info.type) {

	case NAUTILUS_PREFERENCE_BOOLEAN:
		pref_hash_info->pref_value = (gpointer) pref_value;
		gnome_config_set_bool (name, GPOINTER_TO_INT (pref_value));
		break;

	case NAUTILUS_PREFERENCE_ENUM:
		pref_hash_info->pref_value = (gpointer) pref_value;
		gnome_config_set_int (name, GPOINTER_TO_INT (pref_value));
		break;

	case NAUTILUS_PREFERENCE_STRING:
		pref_hash_info->pref_value = g_strdup (pref_value);
		gnome_config_set_string (name, pref_hash_info->pref_value);
		break;
	}

	/* Sync all the damn time.  Yes it sucks.  it will be better with gconf */
	gnome_config_sync ();

	if (pref_hash_info->callback_list) {
		g_list_foreach (pref_hash_info->callback_list,
				pref_callback_info_invoke_func,
				(gpointer) prefs);
	}
}

static gboolean
prefs_get_pref (NautilusPreferences *prefs,
		const char *name,
		NautilusPreferencesType *type_out,
		gconstpointer *value_out)
{
	PrefHashInfo *pref_hash_info;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES (prefs), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (type_out != NULL, FALSE);
	g_return_val_if_fail (value_out != NULL, FALSE);

	pref_hash_info = prefs_hash_lookup (prefs, name);
	if (pref_hash_info == NULL) {
		return FALSE;
	}

	*type_out = pref_hash_info->pref_info.type;
	*value_out = pref_hash_info->pref_value;

	return TRUE;
}

/*
 * NautilusPreferences public methods
 */
GtkObject *
nautilus_preferences_new (const char *domain)
{
	NautilusPreferences *prefs;

	g_return_val_if_fail (domain != NULL, NULL);

	prefs = gtk_type_new (nautilus_preferences_get_type ());

	return GTK_OBJECT (prefs);
}

void
nautilus_preferences_register_from_info (NautilusPreferences *prefs,
					 const NautilusPreferencesInfo *pref_info)
{
	char		*gnome_config_string;
	PrefHashInfo	*pref_hash_info;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES (prefs));

	g_return_if_fail (pref_info != NULL);

	g_return_if_fail (pref_info->name != NULL);
	g_return_if_fail (prefs_check_supported_type (pref_info->type));
	
	pref_hash_info = prefs_hash_lookup (prefs, pref_info->name);

	if (pref_hash_info) {
		g_warning ("the '%s' preference is already registered", pref_info->name);
		return;
	}

	pref_hash_info = pref_hash_info_alloc (pref_info);

	g_hash_table_insert (prefs->details->prefs_hash_table,
			     (gpointer) pref_info->name,
			     (gpointer) pref_hash_info);

	gnome_config_string = make_gnome_config_string (pref_info);

	g_assert (gnome_config_string != NULL);

	/* gnome-config for now; in the future gconf */
	switch (pref_hash_info->pref_info.type) {

	case NAUTILUS_PREFERENCE_BOOLEAN:
		pref_hash_info->pref_value = GINT_TO_POINTER (gnome_config_get_bool (gnome_config_string));
		break;

	case NAUTILUS_PREFERENCE_ENUM:
		pref_hash_info->pref_value = GINT_TO_POINTER (gnome_config_get_int (gnome_config_string));
		break;

	case NAUTILUS_PREFERENCE_STRING:
		pref_hash_info->pref_value = gnome_config_get_string (gnome_config_string);
		break;
	}

	g_free (gnome_config_string);

	/* Sync all the damn time.  Yes it sucks.  it will be better with gconf */
	gnome_config_sync ();
}

static char *
make_gnome_config_string (const NautilusPreferencesInfo *pref_info)
{
	char * rv = NULL;
	GString * tmp = NULL;

	g_assert (pref_info != NULL);

	tmp = g_string_new (pref_info->name);
	
	g_string_append (tmp, "=");
	
	switch (pref_info->type) {

	case NAUTILUS_PREFERENCE_BOOLEAN:
		if (GPOINTER_TO_INT (pref_info->default_value)) {
			g_string_append (tmp, "true");
		} else {
			g_string_append (tmp, "false");
		}
		break;

	case NAUTILUS_PREFERENCE_ENUM:
		g_string_sprintfa  (tmp, "%d", GPOINTER_TO_INT (pref_info->default_value));
		break;
	
	case NAUTILUS_PREFERENCE_STRING:
		g_string_append  (tmp, pref_info->default_value);
		break;
	}
	
	g_assert (tmp != NULL);
	g_assert (tmp->str != NULL);

	rv = g_strdup (tmp->str);

	g_string_free (tmp, TRUE);

	return rv;
}

void
nautilus_preferences_register_from_values (NautilusPreferences *prefs,
					   char	*name,
					   char *description,
					   NautilusPreferencesType type,
					   gconstpointer default_value,
					   gpointer data)
{
	NautilusPreferencesInfo pref_info;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES (prefs));

	g_return_if_fail (name != NULL);
	g_return_if_fail (description != NULL);
	g_return_if_fail (prefs_check_supported_type (type));

	pref_info.name = name;
	pref_info.description = description;
	pref_info.type = type;
	pref_info.default_value = default_value;
	pref_info.data = data;

	nautilus_preferences_register_from_info (prefs, &pref_info);
}

const NautilusPreferencesInfo *
nautilus_preferences_get_pref_info (NautilusPreferences          *prefs,
				    const char            *name)
{
	PrefHashInfo * pref_hash_info;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES (prefs), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	pref_hash_info = prefs_hash_lookup (prefs, name);
	
	g_assert (pref_hash_info != NULL);

	return &pref_hash_info->pref_info;
}

PrefHashInfo *
prefs_hash_lookup (NautilusPreferences   *prefs,
		   const char           *name)
{
	gpointer hash_value;

	g_assert (prefs != NULL);
	g_assert (name != NULL);

	hash_value = g_hash_table_lookup (prefs->details->prefs_hash_table,
					  (gconstpointer) name);

	return (PrefHashInfo *) hash_value;
}

gboolean
nautilus_preferences_add_callback (NautilusPreferences		*prefs,
				   const char			*name,
				   NautilusPreferencesCallback  callback_proc,
				   gpointer			user_data)
{
	PrefHashInfo *pref_hash_info;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES (prefs), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (callback_proc != NULL, FALSE);

	pref_hash_info = prefs_hash_lookup (prefs, name);
	if (pref_hash_info == NULL) {
		g_warning ("trying to add a callback for an unregistered preference");
		return FALSE;
	}

	pref_hash_info_add_callback (pref_hash_info,
				     callback_proc,
				     user_data);

	return TRUE;
}

gboolean
nautilus_preferences_remove_callback (NautilusPreferences	   *prefs,
				      const char		   *name,
				      NautilusPreferencesCallback  callback_proc,
				      gpointer			   user_data)
{
	PrefHashInfo *pref_hash_info;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES (prefs), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (callback_proc != NULL, FALSE);

	pref_hash_info = prefs_hash_lookup (prefs, name);
	if (pref_hash_info == NULL) {
		g_warning ("trying to remove a callback for an unregistered preference");
		return FALSE;
	}

	pref_hash_info_remove_callback (pref_hash_info,
					callback_proc,
					user_data);

	return TRUE;
}

void
nautilus_preferences_set_boolean (NautilusPreferences     *prefs,
				  const char		  *name,
				  gboolean		  boolean_value)
{
	g_return_if_fail (NAUTILUS_IS_PREFERENCES (prefs));
	g_return_if_fail (name != NULL);

	prefs_set_pref (prefs, name, GINT_TO_POINTER (boolean_value));
}

gboolean
nautilus_preferences_get_boolean (NautilusPreferences  *prefs,
				  const char    *name)
{
	gboolean		rv;
	NautilusPreferencesType	type;
	gconstpointer           value;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES (prefs), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	rv = prefs_get_pref (prefs, name, &type, &value);
	if (!rv) {
		g_warning ("tried to get an unregistered boolean preference '%s'", name);
		return FALSE;
	}

	g_assert (type == NAUTILUS_PREFERENCE_BOOLEAN);

	return GPOINTER_TO_INT (value);
}

void
nautilus_preferences_set_enum (NautilusPreferences	*prefs,
			       const char    *name,
			       int           enum_value)
{
	g_return_if_fail (NAUTILUS_IS_PREFERENCES (prefs));
	g_return_if_fail (name != NULL);

	prefs_set_pref (prefs, name, GINT_TO_POINTER (enum_value));
}

int
nautilus_preferences_get_enum (NautilusPreferences  *prefs,
			       const char    *name)
{
	gboolean		rv;
	NautilusPreferencesType	type;
	gconstpointer           value;
	
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES (prefs), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	
	rv = prefs_get_pref (prefs, name, &type, &value);
	if (!rv) {
		g_warning ("tried to get an unregistered enum preference '%s'", name);
		return 0;
	}

	g_assert (type == NAUTILUS_PREFERENCE_ENUM);

	return GPOINTER_TO_INT (value);
}

void
nautilus_preferences_set_string (NautilusPreferences *prefs,
				 const char *name,
				 const char *value)
{
	g_return_if_fail (NAUTILUS_IS_PREFERENCES (prefs));
	g_return_if_fail (name != NULL);
	g_return_if_fail (value != NULL);

	prefs_set_pref (prefs, name, value);
}

char *
nautilus_preferences_get_string (NautilusPreferences *prefs,
				 const char *name)
{
	gboolean		rv;
	NautilusPreferencesType	type;
	gconstpointer           value;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES (prefs), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	rv = prefs_get_pref (prefs, name, &type, &value);
	if (!rv) {
		g_warning ("tried to get an unregistered string preference '%s'", name);
		return NULL;
	}

	g_assert (type == NAUTILUS_PREFERENCE_STRING);

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
