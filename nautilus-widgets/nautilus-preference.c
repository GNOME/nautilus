 /* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-preference.c - An object to describe a single Nautilus preference.

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
#include "nautilus-preference.h"

#include <stdio.h>
#include <stdlib.h>

#include <libgnome/gnome-config.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-string-list.h>
#include "nautilus-widgets-self-check-functions.h"

static const char PREFERENCE_NO_DESCRIPTION[] = "No Description";
/*
 * NautilusPreferenceDetail:
 *
 * Private members for NautilusPreference.
 */
struct NautilusPreferenceDetail {
	char			 *name;
	char			 *description;
	gpointer		 default_value;
	NautilusPreferenceType	 type;
	gpointer		 type_info;
};

typedef struct {
	NautilusStringList     *names;
	NautilusStringList     *descriptions;
	GList		       *values;
	guint		       num_entries;
} PreferenceEnumInfo;

/* NautilusPreferenceClass methods */
static void nautilus_preference_initialize_class (NautilusPreferenceClass *klass);
static void nautilus_preference_initialize       (NautilusPreference      *preference);


/* GtkObjectClass methods */
static void nautilus_preference_destroy          (GtkObject               *object);



/* Default value functions */
static void preference_free_default_value        (NautilusPreference      *preference);

/* Type info functions */
static void preference_free_type_info        (NautilusPreference      *preference);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusPreference, nautilus_preference, GTK_TYPE_OBJECT)

/**
 * nautilus_preference_initialize_class
 *
 * NautilusPreferenceClass class initialization method.
 * @preference_class: The class to initialize.
 *
 **/
static void
nautilus_preference_initialize_class (NautilusPreferenceClass *preference_class)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (preference_class);

 	parent_class = gtk_type_class (gtk_object_get_type ());

	/* GtkObjectClass */
	object_class->destroy = nautilus_preference_destroy;
}

/**
 * nautilus_preference_initialize
 *
 * GtkObject initialization method.
 * @object: The NautilusPreference object to initialize.
 *
 **/
static void
nautilus_preference_initialize (NautilusPreference *preference)
{
	preference->detail = g_new (NautilusPreferenceDetail, 1);

	preference->detail->name = NULL;

	preference->detail->description = NULL;

	preference->detail->default_value = NULL;

	preference->detail->type_info = NULL;

	preference->detail->type = NAUTILUS_PREFERENCE_STRING;
}

/**
 * nautilus_preference_destroy
 *
 * GtkObject destruction method.  Chains to super class.
 * @object: The NautilusPreference object to destroy.
 *
 **/
static void
nautilus_preference_destroy (GtkObject *object)
{
	NautilusPreference *preference;
	
	preference = NAUTILUS_PREFERENCE (object);
	
	if (preference->detail->name != NULL)
		g_free (preference->detail->name);

	if (preference->detail->description != NULL)
		g_free (preference->detail->description);

	preference_free_default_value (preference);

	preference_free_type_info (preference);
	
	g_free (preference->detail);
	
	/* Chain */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/**
 * preference_free_default_value
 *
 * Free the default value according to the preference type
 * @preference: Pointer to self.
 *
 **/
static void
preference_free_default_value (NautilusPreference *preference)
{
	g_assert (preference != NULL);
	g_assert (NAUTILUS_IS_PREFERENCE (preference));
	
	switch (preference->detail->type)
	{
	case NAUTILUS_PREFERENCE_STRING:
		if (preference->detail->default_value)
			g_free (preference->detail->default_value);
		preference->detail->default_value = NULL;
		break;

	default:
		break;
	}
	
	preference->detail->default_value = NULL;
}

/**
 * preference_free_type_info
 *
 * Free the type specific info attatched to this type
 * @preference: Pointer to self.
 *
 **/
static void
preference_free_type_info (NautilusPreference *preference)
{
	g_assert (preference != NULL);
	g_assert (NAUTILUS_IS_PREFERENCE (preference));
	
	switch (preference->detail->type)
	{
	case NAUTILUS_PREFERENCE_ENUM:
		if (preference->detail->type_info) {
			PreferenceEnumInfo * info;

			info = (PreferenceEnumInfo *) preference->detail->type_info;
			
			if (info->names)
				nautilus_string_list_free (info->names);

			if (info->descriptions)
				nautilus_string_list_free (info->descriptions);

			if (info->values)
				g_list_free (info->values);

			info->num_entries = 0;

			g_free (preference->detail->type_info);
		}
		break;

	default:
		break;
	}
	
	preference->detail->type_info = NULL;
}

/**
 * preference_type_info_allocate
 *
 * Allocate the type specific info attatched to this type
 * @preference: Pointer to self.
 *
 **/
static void
preference_allocate_type_info (NautilusPreference *preference)
{
	g_assert (NAUTILUS_IS_PREFERENCE (preference));
	
	switch (preference->detail->type)
	{
	case NAUTILUS_PREFERENCE_ENUM:
	        {
			PreferenceEnumInfo * info;
			
			g_assert (preference->detail->type_info == NULL);
			
			info = g_new (PreferenceEnumInfo, 1);
			
			info->names = nautilus_string_list_new ();
			
			info->descriptions = nautilus_string_list_new ();
			
			info->values = NULL;
			
			info->num_entries = 0;
			
			preference->detail->type_info = (gpointer) info;
		}
	        break;
	
	default:
		break;
	}
}

/**
 * nautilus_preference_new
 *
 * Allocate a new preference object.  By default, preferences created with this
 * function will have a type of NAUTILUS_PREFERENCE_STRING.
 * @name: The name of the preference.
 *
 * Return value: A newly allocated preference.
 *
 **/
GtkObject *
nautilus_preference_new (const char *name)
{
	g_return_val_if_fail (name != NULL, NULL);
	
	return nautilus_preference_new_from_type (name, NAUTILUS_PREFERENCE_STRING);
}

/**
 * nautilus_preference_new_from_type
 *
 * Allocate a new preference object with the given type.
 * @name: The name of the preference.
 * @type: The type for the new preference.
 *
 * Return value: A newly allocated preference.
 *
 **/
GtkObject *
nautilus_preference_new_from_type (const char			*name,
				   NautilusPreferenceType	type)
{
	NautilusPreference *preference;

	g_return_val_if_fail (name != NULL, NULL);
	
	preference = gtk_type_new (nautilus_preference_get_type ());

	g_assert (preference != NULL );

	preference->detail->name = g_strdup (name);

	preference->detail->type = type;

	preference_allocate_type_info (preference);

	return GTK_OBJECT (preference);
}

/**
 * nautilus_preference_get_preference_type
 *
 * Get the type for the given preference.
 * @preference: The preference object.
 *
 * Return value: The type for the preference.
 *
 **/
NautilusPreferenceType 
nautilus_preference_get_preference_type (const NautilusPreference *preference)
{
	g_return_val_if_fail (preference != NULL, NAUTILUS_PREFERENCE_STRING);
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCE (preference), NAUTILUS_PREFERENCE_STRING);

	return preference->detail->type;
}

/**
 * nautilus_preference_set_preference_type
 *
 * Set the type for the given preference.  Yes it possible to morph the preference type
 * in midair.  This is actually an important feature so that users of preferences can pull 
 * them out of their assess without having to register them first.  Changing the preference
 * type is guranteed not to leak resources regardless of the old/new types (such as string
 * default values).
 *
 * @preference: The preference object.
 * @type: The new type of the preference.
 *
 **/
void
nautilus_preference_set_preference_type (NautilusPreference *preference,
					 NautilusPreferenceType type)
{
	g_return_if_fail (preference != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFERENCE (preference));

	/* cmon */
	if (preference->detail->type == type)
		return;

	/* Free the old default value and type info */
	preference_free_default_value (preference);
	preference_free_type_info (preference);

	preference->detail->type = type;

	/* allocate the new type info */
	preference_allocate_type_info (preference);
}


/**
 * nautilus_preference_get_name
 *
 * Get a copy of the name for the given preference.
 * @preference: The preference object.
 *
 * Return value: A newly allocated string with the prefrence name.  This function is always
 * guranteed to return a valid newly allocated string (the default is "").
 *
 **/
char *
nautilus_preference_get_name (const NautilusPreference *preference)
{
	g_return_val_if_fail (preference != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCE (preference), NULL);
	
	return g_strdup (preference->detail->name ? preference->detail->name : "");
}

/**
 * nautilus_preference_get_description
 *
 * Get a copy of the description for the given preference.
 * @preference: The preference object.
 *
 * Return value: A newly allocated string with the prefrence description.  This function is always
 * guranteed to return a valid newly allocated string (the default is "").
 *
 **/
char *
nautilus_preference_get_description (const NautilusPreference *preference)
{
	g_return_val_if_fail (preference != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCE (preference), NULL);
	
	return g_strdup (preference->detail->description ? 
			 preference->detail->description : 
			 PREFERENCE_NO_DESCRIPTION);
}

/**
 * nautilus_preference_set_description
 *
 * Set the description for the current preference.
 * @preference: The preference object.
 * @description: The new description string.
 *
 **/
void
nautilus_preference_set_description (NautilusPreference *preference,
				     const char * description)
{
	g_return_if_fail (preference != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFERENCE (preference));
	g_return_if_fail (description != NULL);

	if (preference->detail->description)
		g_free (preference->detail->description);
	
	preference->detail->description = g_strdup (description);
}


/**
 * nautilus_preference_get_default_value
 *
 * Get a copy of the description for the given preference.
 * @preference: The preference object.
 *
 * Return value: A newly allocated string with the prefrence description.  This function is always
 * guranteed to return a valid newly allocated string (the default is "").
 *
 **/
char *
nautilus_preference_get_default_value (const NautilusPreference *preference)
{
	g_return_val_if_fail (preference != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCE (preference), NULL);
	g_return_val_if_fail (preference->detail->type == NAUTILUS_PREFERENCE_STRING, NULL);

	return (char *) nautilus_preference_any_get_default_value (preference);
}

/**
 * nautilus_preference_set_default_value
 *
 * Set the default value for the given preference.
 * @preference: The preference object.
 * @default_value: The default value.
 *
 **/
void 
nautilus_preference_set_default_value (NautilusPreference	*preference,
				       const char             *default_value)
{
	g_return_if_fail (preference != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFERENCE (preference));
	g_return_if_fail (preference->detail->type == NAUTILUS_PREFERENCE_STRING);
	g_return_if_fail (default_value != NULL);

	nautilus_preference_any_set_default_value (preference, (gconstpointer) default_value);
}


/**
 * nautilus_preference_any_get_default_value
 *
 * Get the default value for an enum preference.
 * @preference: The preference object.
 *
 * Return value: The enum default value as an integer.
 *
 **/
gpointer
nautilus_preference_any_get_default_value (const NautilusPreference *preference)
{
	gpointer default_value;

	g_return_val_if_fail (preference != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCE (preference), 0);

	switch (preference->detail->type)
	{
	case NAUTILUS_PREFERENCE_STRING:
		default_value = (gpointer) g_strdup ((char *) (preference->detail->default_value ? 
							       preference->detail->default_value : ""));

		break;

	default:
		default_value = preference->detail->default_value;
		break;
	}

	return default_value;
}

/**
 * nautilus_preference_any_set_default_value
 *
 * Set the default value for an any preferece.  Deals with all types including those that require alloc/free.
 * @preference: The preference object.
 * @default_value: The new default value as generic pointer.
 *
 **/
void
nautilus_preference_any_set_default_value (NautilusPreference	*preference,
					   gconstpointer	default_value)
{
	g_return_if_fail (preference != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFERENCE (preference));

	preference_free_default_value (preference);


	switch (preference->detail->type)
	{
	case NAUTILUS_PREFERENCE_STRING:
		preference->detail->default_value = (gpointer) g_strdup (default_value ? default_value : "");
		break;

	default:
		preference->detail->default_value = (gpointer) default_value;
		break;
	}
}

/**
 * nautilus_preference_enum_get_default_value
 *
 * Get the default value for an enum preference.
 * @preference: The preference object.
 *
 * Return value: The enum default value as an integer.
 *
 **/
gint
nautilus_preference_enum_get_default_value (const NautilusPreference	*preference)
{
	g_return_val_if_fail (preference != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCE (preference), 0);
	g_return_val_if_fail (preference->detail->type == NAUTILUS_PREFERENCE_ENUM, 0);

	return GPOINTER_TO_INT (nautilus_preference_any_get_default_value (preference));
}

/**
 * nautilus_preference_enum_set_default_value
 *
 * Set the default valu for an enum preferece.
 * @preference: The preference object.
 * @default_value: The new default value as an integer.
 *
 **/
void
nautilus_preference_enum_set_default_value (NautilusPreference	*preference,
					    gint default_value)
{
	g_return_if_fail (preference != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFERENCE (preference));
	g_return_if_fail (preference->detail->type == NAUTILUS_PREFERENCE_ENUM);

	nautilus_preference_any_set_default_value (preference, GINT_TO_POINTER (default_value));
}

void
nautilus_preference_enum_add_entry (NautilusPreference	*preference,
				    const char		*entry_name,
				    const char		*entry_description,
				    gint		entry_value)
{
	PreferenceEnumInfo *info;

	g_return_if_fail (preference != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFERENCE (preference));
	g_return_if_fail (preference->detail->type == NAUTILUS_PREFERENCE_ENUM);
	g_return_if_fail (entry_name != NULL);

	g_assert (preference->detail->type_info != NULL);
	
	info = (PreferenceEnumInfo *) preference->detail->type_info;

	nautilus_string_list_insert (info->names, entry_name);

	nautilus_string_list_insert (info->descriptions, 
				     entry_description ? entry_description : PREFERENCE_NO_DESCRIPTION);

	info->values = g_list_append (info->values, GINT_TO_POINTER (entry_value));

	info->num_entries++;
}

char *
nautilus_preference_enum_get_nth_entry_name (const NautilusPreference *preference,
					     guint n)
{
	PreferenceEnumInfo *info;

	g_return_val_if_fail (preference != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCE (preference), NULL);
	g_return_val_if_fail (preference->detail->type == NAUTILUS_PREFERENCE_ENUM, NULL);

	g_assert (preference->detail->type_info != NULL);

	info = (PreferenceEnumInfo *) preference->detail->type_info;

	if (n < nautilus_string_list_get_length (info->names))
		return nautilus_string_list_nth (info->names, n);

	return NULL;
}

char *
nautilus_preference_enum_get_nth_entry_description (const NautilusPreference *preference,
					     guint n)
{
	PreferenceEnumInfo *info;

	g_return_val_if_fail (preference != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCE (preference), NULL);
	g_return_val_if_fail (preference->detail->type == NAUTILUS_PREFERENCE_ENUM, NULL);

	g_assert (preference->detail->type_info != NULL);

	info = (PreferenceEnumInfo *) preference->detail->type_info;

	if (n < nautilus_string_list_get_length (info->descriptions))
		return nautilus_string_list_nth (info->descriptions, n);

	return NULL;
}

gint
nautilus_preference_enum_get_nth_entry_value (const NautilusPreference *preference,
					      guint n)
{
	PreferenceEnumInfo *info;

	g_return_val_if_fail (preference != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCE (preference), 0);
	g_return_val_if_fail (preference->detail->type == NAUTILUS_PREFERENCE_ENUM, 0);

	g_assert (preference->detail->type_info != NULL);

	info = (PreferenceEnumInfo *) preference->detail->type_info;

	if (n < g_list_length (info->values))
		return GPOINTER_TO_INT (g_list_nth_data (info->values, n));

	return 0;
}

guint
nautilus_preference_enum_get_num_entries (const NautilusPreference *preference)
{
	g_return_val_if_fail (preference != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCE (preference), 0);
	g_return_val_if_fail (preference->detail->type == NAUTILUS_PREFERENCE_ENUM, 0);

	if (preference->detail->type_info) {
		PreferenceEnumInfo *info = (PreferenceEnumInfo *) preference->detail->type_info;
		
		return info->num_entries;
	}

	return 0;
}

/**
 * nautilus_preference_boolean_get_default_value
 *
 * Get the default value for an boolean preference.
 * @preference: The preference object.
 *
 * Return value: The boolean default value as an integer.
 *
 **/
gboolean
nautilus_preference_boolean_get_default_value (const NautilusPreference *preference)
{
	g_return_val_if_fail (preference != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_PREFERENCE (preference), 0);
	g_return_val_if_fail (preference->detail->type == NAUTILUS_PREFERENCE_BOOLEAN, 0);

	return (gboolean) nautilus_preference_any_get_default_value (preference);
}

/**
 * nautilus_preference_boolean_set_default_value
 *
 * Set the default valu for an boolean preferece.
 * @preference: The preference object.
 * @default_value: The new default value.
 *
 **/
void
nautilus_preference_boolean_set_default_value (NautilusPreference	*preference,
					       gboolean default_value)
{
	g_return_if_fail (preference != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFERENCE (preference));
	g_return_if_fail (preference->detail->type == NAUTILUS_PREFERENCE_BOOLEAN);

	nautilus_preference_any_set_default_value (preference, (gpointer) (default_value ? 1 : 0));
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_widgets_self_check_preference (void)
{
	NautilusPreference *preference;

	preference = NAUTILUS_PREFERENCE (nautilus_preference_new ("foo/bar"));

	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_preference_get_preference_type (preference), NAUTILUS_PREFERENCE_STRING);

	/* Test boolean things */
	{
		NautilusPreference *bp;
		
		bp = NAUTILUS_PREFERENCE (nautilus_preference_new_from_type ("active", NAUTILUS_PREFERENCE_BOOLEAN));

		nautilus_preference_set_description (bp, "Is it active ?");
		
		nautilus_preference_boolean_set_default_value (bp, TRUE);

		NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_preference_boolean_get_default_value (bp), TRUE);
		
		nautilus_preference_boolean_set_default_value (bp, FALSE);

		NAUTILUS_CHECK_BOOLEAN_RESULT (nautilus_preference_boolean_get_default_value (bp), FALSE);

		NAUTILUS_CHECK_STRING_RESULT (nautilus_preference_get_name (bp), "active");

		NAUTILUS_CHECK_STRING_RESULT (nautilus_preference_get_description (bp), "Is it active ?");
		
		gtk_object_unref (GTK_OBJECT (bp));
	}

	/* Test enumeration things */
	{
		NautilusPreference *ep;
		
		ep = NAUTILUS_PREFERENCE (nautilus_preference_new_from_type ("color",NAUTILUS_PREFERENCE_ENUM));
		
		nautilus_preference_enum_add_entry (ep, "red", "A red something", 100);
		nautilus_preference_enum_add_entry (ep, "green", "A green something", 200);
		nautilus_preference_enum_add_entry (ep, "blue", "A blue something", 300);
		
		NAUTILUS_CHECK_STRING_RESULT (nautilus_preference_get_name (ep), "color");

		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_preference_enum_get_num_entries (ep), 3);

		NAUTILUS_CHECK_STRING_RESULT (nautilus_preference_enum_get_nth_entry_name (ep, 0), "red");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_preference_enum_get_nth_entry_name (ep, 1), "green");
		NAUTILUS_CHECK_STRING_RESULT (nautilus_preference_enum_get_nth_entry_name (ep, 2), "blue");

		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_preference_enum_get_nth_entry_value (ep, 0), 100);
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_preference_enum_get_nth_entry_value (ep, 1), 200);
		NAUTILUS_CHECK_INTEGER_RESULT (nautilus_preference_enum_get_nth_entry_value (ep, 2), 300);

		gtk_object_unref (GTK_OBJECT (ep));
	}

	/* Test string things */
	{
		NautilusPreference *sp;
		
		sp = NAUTILUS_PREFERENCE (nautilus_preference_new_from_type ("font", NAUTILUS_PREFERENCE_STRING));
		
		nautilus_preference_set_default_value (sp, "helvetica");

		NAUTILUS_CHECK_STRING_RESULT (nautilus_preference_get_default_value (sp), "helvetica");

		NAUTILUS_CHECK_STRING_RESULT (nautilus_preference_get_name (sp), "font");

		gtk_object_unref (GTK_OBJECT (sp));
	}


	/* Allocate a bunch of preference objects to test that they dont leak */
	{
		const guint	num_to_allocate = 666;
		guint		i;

		for (i = 0; i < num_to_allocate; i++)
		{
			NautilusPreference *bp;
			char		   *bn;
			
			bn = g_strdup_printf ("bp_%d", i);

			bp = NAUTILUS_PREFERENCE (nautilus_preference_new_from_type (bn, NAUTILUS_PREFERENCE_BOOLEAN));
			
			g_free (bn);

			gtk_object_unref (GTK_OBJECT (bp));
		}

		for (i = 0; i < num_to_allocate; i++)
		{
			NautilusPreference *ep;
			char		   *en;

			en = g_strdup_printf ("ep_%d", i);

			ep = NAUTILUS_PREFERENCE (nautilus_preference_new_from_type (en, NAUTILUS_PREFERENCE_ENUM));
			
			nautilus_preference_enum_add_entry (ep, "small", "A small foo", 1);
			nautilus_preference_enum_add_entry (ep, "medium", "A medium foo", 2);
			nautilus_preference_enum_add_entry (ep, "large", "A large foo", 3);

			g_free (en);

			gtk_object_unref (GTK_OBJECT (ep));
		}

		for (i = 0; i < num_to_allocate; i++)
		{
			NautilusPreference *sp;
			char		   *sn;
			char		   *string_default_value;
			
			sn = g_strdup_printf ("sp_%d", i);

			string_default_value = g_strdup_printf ("default_value_%d", i);

			sp = NAUTILUS_PREFERENCE (nautilus_preference_new_from_type (sn, NAUTILUS_PREFERENCE_STRING));

			nautilus_preference_set_default_value (sp, string_default_value);
			
			g_free (sn);
			g_free (string_default_value);

			gtk_object_unref (GTK_OBJECT (sp));
		}
	}
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
