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


#include <nautilus-widgets/nautilus-preferences.h>
#include <libnautilus/nautilus-gtk-macros.h>

static const gchar * PREFERENCES_GLOBAL_DOMAIN = "Nautilus::Global";

#include <stdio.h>

enum
{
	ACTIVATE,
	LAST_SIGNAL
};

typedef struct
{
	NautilusPreferencesInfo	pref_info;
	gpointer		pref_value;
	GList			*callback_list;
} PrefHashInfo;

typedef struct
{
	NautilusPreferencesCallback	callback_proc;
	gpointer		user_data;
	const PrefHashInfo	*hash_info;
} PrefCallbackInfo;

struct _NautilusPreferencesDetails
{
	gchar		*domain;
	GHashTable	*prefs_hash_table;
};

/* NautilusPreferencesClass methods */
static void               nautilus_preferences_initialize_class (NautilusPreferencesClass      *klass);
static void               nautilus_preferences_initialize       (NautilusPreferences           *prefs);

/* GtkObjectClass methods */
static void               prefs_destroy                         (GtkObject                     *object);

/* PrefHashInfo functions */
static PrefHashInfo *     pref_hash_info_alloc                  (const NautilusPreferencesInfo *pref_info);
static void               pref_hash_info_free                   (PrefHashInfo                  *pref_hash_info);
static void               pref_hash_info_free_func              (gpointer                       key,
								 gpointer                       value,
								 gpointer                       user_data);

/* PrefCallbackInfo functions */
static PrefCallbackInfo * pref_callback_info_alloc              (NautilusPreferencesCallback    callback_proc,
								 gpointer                       user_data,
								 const PrefHashInfo            *hash_info);
static void               pref_callback_info_free               (PrefCallbackInfo              *pref_hash_info);
static void               pref_callback_info_free_func          (gpointer                       data,
								 gpointer                       user_data);
static void               pref_callback_info_invoke_func        (gpointer                       data,
								 gpointer                       user_data);
static void               pref_hash_info_add_callback           (PrefHashInfo                  *pref_hash_info,
								 NautilusPreferencesCallback    callback_proc,
								 gpointer                       user_data);
static void               pref_hash_info_remove_callback        (PrefHashInfo                  *pref_hash_info,
								 NautilusPreferencesCallback    callback_proc,
								 gpointer                       user_data);

/* Private stuff */
static NautilusPreferencesType prefs_check_supported_type            (NautilusPreferencesType             pref_type);
static gboolean           prefs_set_pref                        (NautilusPreferences           *prefs,
								 const gchar                   *pref_name,
								 gpointer                       pref_value);
static gboolean           prefs_get_pref                        (NautilusPreferences           *prefs,
								 const gchar                   *pref_name,
								 NautilusPreferencesType            *pref_type_out,
								 gconstpointer                 *pref_value_out);
PrefHashInfo *            prefs_hash_lookup                     (NautilusPreferences           *prefs,
								 const gchar                   *pref_name);

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
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS (object));
	
	prefs = NAUTILUS_PREFS (object);

	if (prefs->details->domain)
	{
		g_free (prefs->details->domain);
		prefs->details->domain = NULL;
	}

	if (prefs->details->prefs_hash_table)
	{
		g_hash_table_foreach (prefs->details->prefs_hash_table,
				      pref_hash_info_free_func,
				      (gpointer) NULL);
		
		g_hash_table_destroy (prefs->details->prefs_hash_table);

		prefs->details->prefs_hash_table = NULL;
	}

	g_free (prefs->details);
	
	/* Chain */
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/*
 * PrefHashInfo functions
 */
static PrefHashInfo *
pref_hash_info_alloc (const NautilusPreferencesInfo *pref_info)
{
	PrefHashInfo * pref_hash_info;
	
	g_assert (pref_info != NULL);

	g_assert (pref_info->pref_name != NULL);
	g_assert (pref_info->pref_description != NULL);

	pref_hash_info = g_new (PrefHashInfo, 1);

	pref_hash_info->pref_info.pref_name = g_strdup (pref_info->pref_name);
	pref_hash_info->pref_info.pref_description = g_strdup (pref_info->pref_description);
	pref_hash_info->pref_info.pref_type = pref_info->pref_type;
	pref_hash_info->pref_info.pref_default_value = pref_info->pref_default_value;
	pref_hash_info->pref_info.type_data = pref_info->type_data;

	pref_hash_info->pref_value = (gpointer) pref_info->pref_default_value;
	pref_hash_info->callback_list = NULL;

	return pref_hash_info;
}

static void
pref_hash_info_free (PrefHashInfo *pref_hash_info)
{
	g_assert (pref_hash_info != NULL);

	g_assert (pref_hash_info->pref_info.pref_name != NULL);
	g_assert (pref_hash_info->pref_info.pref_description != NULL);

	if (pref_hash_info->callback_list)
	{
		g_list_foreach (pref_hash_info->callback_list,
				pref_callback_info_free_func,
				(gpointer) NULL);
	}
	
	g_free (pref_hash_info->pref_info.pref_name);
	g_free (pref_hash_info->pref_info.pref_description);

	pref_hash_info->pref_info.pref_name = NULL;
	pref_hash_info->pref_info.pref_type = GTK_TYPE_INVALID;
	pref_hash_info->pref_info.pref_default_value = NULL;
	pref_hash_info->pref_info.type_data = NULL;

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

	for (iterator = new_list; iterator != NULL; iterator = iterator->next) 
	{
		PrefCallbackInfo *callback_info = (PrefCallbackInfo *) iterator->data;

		g_assert (callback_info != NULL);

		if (callback_info->callback_proc == callback_proc &&
		    callback_info->user_data == user_data)
		{
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
 	const NautilusPreferences     *prefs;
	PrefCallbackInfo *pref_callback_info;

	pref_callback_info = (PrefCallbackInfo *) data;

	g_assert (pref_callback_info != NULL);

	g_assert (pref_callback_info->callback_proc != NULL);

 	prefs = (const NautilusPreferences *) user_data;

 	(*pref_callback_info->callback_proc) (GTK_OBJECT (prefs),
					      pref_callback_info->hash_info->pref_info.pref_name,
					      pref_callback_info->hash_info->pref_info.pref_type,
					      pref_callback_info->hash_info->pref_value,
					      pref_callback_info->user_data);
}

/*
 * Private stuff
 */
static NautilusPreferencesType
prefs_check_supported_type (NautilusPreferencesType pref_type)
{
	return ((pref_type == NAUTILUS_PREFERENCE_BOOLEAN) ||
		(pref_type == NAUTILUS_PREFERENCE_ENUM));
}

static gboolean
prefs_set_pref (NautilusPreferences     *prefs,
		const gchar       *pref_name,
		gpointer          pref_value)
{
	PrefHashInfo * pref_hash_info;

	g_return_val_if_fail (prefs != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_PREFS (prefs), FALSE);
	g_return_val_if_fail (pref_name != NULL, FALSE);

	pref_hash_info = prefs_hash_lookup (prefs, pref_name);

	if (!pref_hash_info)
	{
		return FALSE;
	}

	/*
	 * XXX FIXME: When we support GTK_TYPE_STRING, this
	 * will have to strcpy() the given value
	 */
	pref_hash_info->pref_value = pref_value;

	/* gnome-config for now ; in the future gconf */
	switch (pref_hash_info->pref_info.pref_type)
	{
	case NAUTILUS_PREFERENCE_BOOLEAN:
		gnome_config_set_bool (pref_name, (gboolean) pref_hash_info->pref_value);
		break;

	case NAUTILUS_PREFERENCE_ENUM:
		gnome_config_set_int (pref_name, (gint) pref_hash_info->pref_value);
		break;
	}

	/* Sync all the damn time.  Yes it sucks.  it will be better with gconf */
	gnome_config_sync ();

	if (pref_hash_info->callback_list)
	{
		g_list_foreach (pref_hash_info->callback_list,
				pref_callback_info_invoke_func,
				(gpointer) prefs);
	}

	return TRUE;
}

static gboolean
prefs_get_pref (NautilusPreferences         *prefs,
		const gchar           *pref_name,
		NautilusPreferencesType    *pref_type_out,
		gconstpointer         *pref_value_out)
{
	PrefHashInfo * pref_hash_info;

	g_return_val_if_fail (prefs != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_PREFS (prefs), FALSE);
	g_return_val_if_fail (pref_type_out != NULL, FALSE);
	g_return_val_if_fail (pref_value_out != NULL, FALSE);

	if (!pref_hash_info)
	{
		return FALSE;
	}

	pref_hash_info = prefs_hash_lookup (prefs, pref_name);

	g_assert (pref_hash_info != NULL);

	*pref_type_out = pref_hash_info->pref_info.pref_type;
	*pref_value_out = pref_hash_info->pref_value;

	return TRUE;
}

/*
 * NautilusPreferences public methods
 */
GtkObject *
nautilus_preferences_new (const gchar *domain)
{
	NautilusPreferences *prefs;

	g_return_val_if_fail (domain != NULL, NULL);

	prefs = gtk_type_new (nautilus_preferences_get_type ());

	return GTK_OBJECT (prefs);
}

void
nautilus_preferences_register_from_info (NautilusPreferences		*prefs,
					 const NautilusPreferencesInfo *pref_info)
{
	PrefHashInfo * pref_hash_info;

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS (prefs));

	g_return_if_fail (pref_info != NULL);

	g_return_if_fail (pref_info->pref_name != NULL);
	g_return_if_fail (prefs_check_supported_type (pref_info->pref_type));
	
	pref_hash_info = prefs_hash_lookup (prefs, pref_info->pref_name);

	if (pref_hash_info)
	{
		g_warning ("the '%s' preference is already registered.\n", pref_info->pref_name);

		return;
	}

	pref_hash_info = pref_hash_info_alloc (pref_info);

	g_hash_table_insert (prefs->details->prefs_hash_table,
			     (gpointer) pref_info->pref_name,
			     (gpointer) pref_hash_info);

	/* gnome-config for now ; in the future gconf */
	switch (pref_hash_info->pref_info.pref_type)
	{
	case NAUTILUS_PREFERENCE_BOOLEAN:
		pref_hash_info->pref_value = (gpointer) gnome_config_get_bool (pref_info->pref_name);
		break;

	case NAUTILUS_PREFERENCE_ENUM:
		pref_hash_info->pref_value = (gpointer) gnome_config_get_int (pref_info->pref_name);
		break;
	}

	/* Sync all the damn time.  Yes it sucks.  it will be better with gconf */
	gnome_config_sync ();
}

void
nautilus_preferences_register_from_values (NautilusPreferences      *prefs,
					   gchar		*pref_name,
					   gchar		*pref_description,
					   NautilusPreferencesType	pref_type,
					   gconstpointer	pref_default_value,
					   gpointer		type_data)
{
	NautilusPreferencesInfo pref_info;

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS (prefs));

	g_return_if_fail (pref_name != NULL);
	g_return_if_fail (pref_description != NULL);
	g_return_if_fail (prefs_check_supported_type (pref_type));

	pref_info.pref_name = pref_name;
	pref_info.pref_description = pref_description;
	pref_info.pref_type = pref_type;
	pref_info.pref_default_value = pref_default_value;
	pref_info.type_data = type_data;

	nautilus_preferences_register_from_info (prefs, &pref_info);
}

const NautilusPreferencesInfo *
nautilus_preferences_get_pref_info (NautilusPreferences          *prefs,
				    const gchar            *pref_name)
{
	PrefHashInfo * pref_hash_info;

	g_return_val_if_fail (prefs != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_PREFS (prefs), NULL);
	g_return_val_if_fail (pref_name != NULL, NULL);

	pref_hash_info = prefs_hash_lookup (prefs, pref_name);
	
	g_assert (pref_hash_info != NULL);

	return &pref_hash_info->pref_info;
}

PrefHashInfo *
prefs_hash_lookup (NautilusPreferences   *prefs,
		   const gchar           *pref_name)
{
	gpointer hash_value;

	g_assert (prefs != NULL);
	g_assert (pref_name != NULL);

	hash_value = g_hash_table_lookup (prefs->details->prefs_hash_table,
					  (gconstpointer) pref_name);

	return (PrefHashInfo *) hash_value;
}

gboolean
nautilus_preferences_add_callback (NautilusPreferences		*prefs,
				   const gchar			*pref_name,
				   NautilusPreferencesCallback  callback_proc,
				   gpointer			user_data)
{
	PrefHashInfo * pref_hash_info;

	g_return_val_if_fail (prefs != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_PREFS (prefs), FALSE);
	g_return_val_if_fail (pref_name != NULL, FALSE);
	g_return_val_if_fail (callback_proc != NULL, FALSE);

	pref_hash_info = prefs_hash_lookup (prefs, pref_name);

	if (!pref_hash_info)
	{
		g_warning ("trying to add a callback to an unregistered preference.\n");

		return FALSE;
	}

	pref_hash_info_add_callback (pref_hash_info,
				     callback_proc,
				     user_data);

	return TRUE;
}

gboolean
nautilus_preferences_remove_callback (NautilusPreferences	   *prefs,
				      const gchar		   *pref_name,
				      NautilusPreferencesCallback  callback_proc,
				      gpointer			   user_data)
{
	PrefHashInfo * pref_hash_info;

	g_return_val_if_fail (prefs != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_PREFS (prefs), FALSE);
	g_return_val_if_fail (pref_name != NULL, FALSE);
	g_return_val_if_fail (callback_proc != NULL, FALSE);

	pref_hash_info = prefs_hash_lookup (prefs, pref_name);

	if (!pref_hash_info)
	{
		g_warning ("trying to remove a callback from an unregistered preference.\n");

		return FALSE;
	}

	pref_hash_info_remove_callback (pref_hash_info,
					callback_proc,
					user_data);

	return TRUE;
}

void
nautilus_preferences_set_boolean (NautilusPreferences     *prefs,
				  const gchar		  *pref_name,
				  gboolean		  boolean_value)
{
	gboolean rv;

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS (prefs));
	g_return_if_fail (pref_name != NULL);

	rv = prefs_set_pref (prefs, pref_name, (gpointer) boolean_value);

	g_assert (rv);
}

gboolean
nautilus_preferences_get_boolean (NautilusPreferences  *prefs,
				  const gchar    *pref_name)
{
	gboolean		rv;
	NautilusPreferencesType	pref_type;
	gconstpointer           value;

	g_return_val_if_fail (prefs != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_PREFS (prefs), FALSE);
	g_return_val_if_fail (pref_name != NULL, FALSE);

	rv = prefs_get_pref (prefs, pref_name, &pref_type, &value);

	if (!rv)
	{
		g_warning ("could not get boolean preference '%s'\n", pref_name);

		return FALSE;
	}

	g_assert (pref_type == NAUTILUS_PREFERENCE_BOOLEAN);

	return (gboolean) value;
}

void
nautilus_preferences_set_enum (NautilusPreferences	*prefs,
			       const gchar    *pref_name,
			       gint           enum_value)
{
	gboolean rv;

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS (prefs));
	g_return_if_fail (pref_name != NULL);

	rv = prefs_set_pref (prefs, pref_name, (gpointer) enum_value);

	g_assert (rv);
}

gint
nautilus_preferences_get_enum (NautilusPreferences  *prefs,
			       const gchar    *pref_name)
{
	gboolean		rv;
	NautilusPreferencesType	pref_type;
	gconstpointer           value;
	
	g_return_val_if_fail (prefs != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_PREFS (prefs), FALSE);
	g_return_val_if_fail (pref_name != NULL, FALSE);
	
	rv = prefs_get_pref (prefs, pref_name, &pref_type, &value);

	if (!rv)
	{
		g_warning ("could not get enum preference '%s'\n", pref_name);

		return FALSE;
	}

	g_assert (pref_type == NAUTILUS_PREFERENCE_ENUM);

	return (gint) value;
}

NautilusPreferences *
nautilus_preferences_get_global_preferences (void)
{
	static GtkObject * global_preferences = NULL;

	if (!global_preferences)
	{
		global_preferences = nautilus_preferences_new (PREFERENCES_GLOBAL_DOMAIN);
	}

	return NAUTILUS_PREFS (global_preferences);
}
