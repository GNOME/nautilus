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

#include <gtk/gtkclist.h>

// #include <gtk/gtkmain.h>

#include <libgnomeui/gnome-stock.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

#include <stdio.h>

enum
{
	ACTIVATE,
	LAST_SIGNAL
};

typedef struct
{
	NautilusPrefInfo	pref_info;
	gpointer		pref_value;
	GList			*callback_list;
} PrefHashInfo;

typedef struct
{
	NautilusPrefsCallback	callback_proc;
	gpointer		user_data;
	const PrefHashInfo	*hash_info;
} PrefCallbackInfo;

struct _NautilusPrefsPrivate
{
	gchar		*domain;
	GHashTable	*prefs_hash_table;
};

/* NautilusPrefsClass methods */
static void               nautilus_prefs_initialize_class (NautilusPrefsClass     *klass);
static void               nautilus_prefs_initialize       (NautilusPrefs          *prefs);




/* GtkObjectClass methods */
static void               prefs_destroy                   (GtkObject              *object);




/* PrefHashInfo functions */
static PrefHashInfo *     pref_hash_info_alloc            (const NautilusPrefInfo *pref_info);
static void               pref_hash_info_free             (PrefHashInfo           *pref_hash_info);
static void               pref_hash_info_free_func        (gpointer                key,
							   gpointer                value,
							   gpointer                user_data);

/* PrefCallbackInfo functions */
static PrefCallbackInfo * pref_callback_info_alloc        (NautilusPrefsCallback   callback_proc,
							   gpointer user_data,
							   const PrefHashInfo	*hash_info);
static void               pref_callback_info_free         (PrefCallbackInfo       *pref_hash_info);
static void               pref_callback_info_free_func    (gpointer                data,
							   gpointer                user_data);
static void               pref_callback_info_invoke_func    (gpointer                data,
							   gpointer                user_data);
static void               pref_hash_info_add_callback     (PrefHashInfo           *pref_hash_info,
							   NautilusPrefsCallback   callback_proc,
							   gpointer                user_data);

/* Private stuff */
static GtkFundamentalType prefs_check_supported_type      (GtkFundamentalType      pref_type);
static gboolean           prefs_set_pref                  (NautilusPrefs          *prefs,
							   const gchar            *pref_name,
							   gpointer		  pref_value);
static gboolean           prefs_get_pref                  (NautilusPrefs          *prefs,
							   const gchar            *pref_name,
							   GtkFundamentalType     *pref_type_out,
							   gconstpointer          *pref_value_out);
PrefHashInfo *            prefs_hash_lookup               (NautilusPrefs          *prefs,
							   const gchar            *pref_name);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusPrefs, nautilus_prefs, GTK_TYPE_OBJECT)

/*
 * NautilusPrefsClass methods
 */
static void
nautilus_prefs_initialize_class (NautilusPrefsClass *prefs_class)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (prefs_class);

 	parent_class = gtk_type_class (gtk_object_get_type ());

	/* GtkObjectClass */
	object_class->destroy = prefs_destroy;
}

static void
nautilus_prefs_initialize (NautilusPrefs *prefs)
{
	prefs->priv = g_new (NautilusPrefsPrivate, 1);

	prefs->priv->domain = NULL;

	prefs->priv->prefs_hash_table = g_hash_table_new (g_str_hash, 
							  g_str_equal);
}

/*
 * GtkObjectClass methods
 */
static void
prefs_destroy (GtkObject *object)
{
	NautilusPrefs * prefs;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS (object));
	
	prefs = NAUTILUS_PREFS (object);

	if (prefs->priv->domain)
	{
		g_free (prefs->priv->domain);
		prefs->priv->domain = NULL;
	}

	if (prefs->priv->prefs_hash_table)
	{
		g_hash_table_foreach (prefs->priv->prefs_hash_table,
				      pref_hash_info_free_func,
				      (gpointer) NULL);
		
		g_hash_table_destroy (prefs->priv->prefs_hash_table);

		prefs->priv->prefs_hash_table = NULL;
	}

	g_free (prefs->priv);
	
	/* Chain */
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/*
 * PrefHashInfo functions
 */
static PrefHashInfo *
pref_hash_info_alloc (const NautilusPrefInfo *pref_info)
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
			     NautilusPrefsCallback	callback_proc,
			     gpointer			user_data)
{
	PrefCallbackInfo * pref_callback_info;

	g_assert (pref_hash_info != NULL);

	g_assert (callback_proc != NULL);

	pref_callback_info = pref_callback_info_alloc (callback_proc, 
						       user_data,
						       pref_hash_info);

	pref_hash_info->callback_list =
		g_list_append (pref_hash_info->callback_list, 
			       (gpointer) pref_callback_info);
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
pref_callback_info_alloc (NautilusPrefsCallback	callback_proc,
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
 	const NautilusPrefs     *prefs;
	PrefCallbackInfo *pref_callback_info;

	pref_callback_info = (PrefCallbackInfo *) data;

	g_assert (pref_callback_info != NULL);

	g_assert (pref_callback_info->callback_proc != NULL);

 	prefs = (const NautilusPrefs *) user_data;

 	(*pref_callback_info->callback_proc) (GTK_OBJECT (prefs),
					      pref_callback_info->hash_info->pref_info.pref_name,
					      pref_callback_info->hash_info->pref_info.pref_type,
					      pref_callback_info->hash_info->pref_value,
					      pref_callback_info->user_data);
}

/*
 * Private stuff
 */
static GtkFundamentalType
prefs_check_supported_type (GtkFundamentalType pref_type)
{
	return ((pref_type == GTK_TYPE_BOOL) ||
		(pref_type == GTK_TYPE_INT) ||
		(pref_type == GTK_TYPE_ENUM));
}

static gboolean
prefs_set_pref (NautilusPrefs     *prefs,
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

	if (pref_hash_info->callback_list)
	{
		g_list_foreach (pref_hash_info->callback_list,
				pref_callback_info_invoke_func,
				(gpointer) prefs);
	}

	return TRUE;
}

static gboolean
prefs_get_pref (NautilusPrefs         *prefs,
		const gchar           *pref_name,
		GtkFundamentalType    *pref_type_out,
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

//	printf("looking for %s\n", pref_name);

	g_assert (pref_hash_info != NULL);

// 	printf("pref_name = %s\n", pref_hash_info->pref_info.pref_name);
// 	printf("pref_desc = %s\n", pref_hash_info->pref_info.pref_description);
// 	printf("pref_type = %d\n", pref_hash_info->pref_info.pref_type);

	*pref_type_out = pref_hash_info->pref_info.pref_type;
	*pref_value_out = pref_hash_info->pref_value;

	return TRUE;
}

/*
 * NautilusPrefs public methods
 */
GtkObject *
nautilus_prefs_new (const gchar *domain)
{
	NautilusPrefs *prefs;

	g_return_val_if_fail (domain != NULL, NULL);

	prefs = gtk_type_new (nautilus_prefs_get_type ());

	return GTK_OBJECT (prefs);
}

void
nautilus_prefs_register_from_info (NautilusPrefs		*prefs,
				   const NautilusPrefInfo *pref_info)
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

// 	printf ("%s is being registered\n", pref_info->pref_name);

	pref_hash_info = pref_hash_info_alloc (pref_info);

	g_hash_table_insert (prefs->priv->prefs_hash_table,
			     (gpointer) pref_info->pref_name,
			     (gpointer) pref_hash_info);
}

void
nautilus_prefs_register_from_values (NautilusPrefs      *prefs,
				     gchar		*pref_name,
				     gchar		*pref_description,
				     GtkFundamentalType	pref_type,
				     gconstpointer	pref_default_value,
				     gpointer		type_data)
{
	NautilusPrefInfo pref_info;

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

	nautilus_prefs_register_from_info (prefs, &pref_info);
}

const NautilusPrefInfo *
nautilus_prefs_get_pref_info (NautilusPrefs          *prefs,
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
prefs_hash_lookup (NautilusPrefs         *prefs,
		   const gchar           *pref_name)
{
	gpointer hash_value;

	g_assert (prefs != NULL);
	g_assert (pref_name != NULL);

	hash_value = g_hash_table_lookup (prefs->priv->prefs_hash_table,
					  (gconstpointer) pref_name);

	return (PrefHashInfo *) hash_value;
}

gboolean
nautilus_prefs_add_callback (NautilusPrefs         *prefs,
			     const gchar           *pref_name,
			     NautilusPrefsCallback  callback_proc,
			     gpointer               user_data)
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

void
nautilus_prefs_set_boolean (NautilusPrefs     *prefs,
			    const gchar       *pref_name,
			    gboolean          boolean_value)
{
	gboolean rv;

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS (prefs));
	g_return_if_fail (pref_name != NULL);

	rv = prefs_set_pref (prefs, pref_name, (gpointer) boolean_value);

	g_assert (rv);
}

gboolean
nautilus_prefs_get_boolean (NautilusPrefs  *prefs,
			    const gchar    *pref_name)
{
	gboolean		rv;
	GtkFundamentalType	pref_type;
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

	g_assert (pref_type == GTK_TYPE_BOOL);

	return (gboolean) value;
}

void
nautilus_prefs_set_enum (NautilusPrefs	*prefs,
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
nautilus_prefs_get_enum (NautilusPrefs  *prefs,
			 const gchar    *pref_name)
{
	gboolean		rv;
	GtkFundamentalType	pref_type;
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

	g_assert (pref_type == GTK_TYPE_ENUM);

	return (gint) value;
}
