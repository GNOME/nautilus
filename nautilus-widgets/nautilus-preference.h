/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-preference.h - An object to describe a single Nautilus preference.

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

#ifndef NAUTILUS_PREFERENCE_H
#define NAUTILUS_PREFERENCE_H

#include <gtk/gtkobject.h>
#include <libgnome/gnome-defs.h>
#include <libnautilus-extensions/nautilus-string-list.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_PREFERENCE            (nautilus_preference_get_type ())
#define NAUTILUS_PREFERENCE(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_PREFERENCE, NautilusPreference))
#define NAUTILUS_PREFERENCE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PREFERENCE, NautilusPreferenceClass))
#define NAUTILUS_IS_PREFERENCE(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_PREFERENCE))
#define NAUTILUS_IS_PREFERENCE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PREFERENCE))

typedef struct NautilusPreference	    NautilusPreference;
typedef struct NautilusPreferenceClass      NautilusPreferenceClass;
typedef struct NautilusPreferenceDetail     NautilusPreferenceDetail;

struct NautilusPreference
{
	GtkObject			object;
	NautilusPreferenceDetail	*detail;
};

/*
 * NautilusPreferenceType:
 *
 * The types of supported preferences.  By default, a preference is of type 
 * NAUTILUS_PREFERENCE_STRING, unless otherwise specified in the api.
 */
typedef enum
{
	NAUTILUS_PREFERENCE_STRING,
	NAUTILUS_PREFERENCE_BOOLEAN,
	NAUTILUS_PREFERENCE_ENUM
} NautilusPreferenceType;

struct NautilusPreferenceClass
{
	GtkObjectClass object_class;
};

GtkType                nautilus_preference_get_type                       (void);
GtkObject *            nautilus_preference_new                            (const char               *name);
GtkObject *            nautilus_preference_new_from_type                  (const char               *name,
									   NautilusPreferenceType    type);
NautilusPreferenceType nautilus_preference_get_preference_type            (const NautilusPreference *preference);
void                   nautilus_preference_set_preference_type            (NautilusPreference       *preference,
									   NautilusPreferenceType    type);
char *                 nautilus_preference_get_name                       (const NautilusPreference *preference);
char *                 nautilus_preference_get_description                (const NautilusPreference *preference);
void                   nautilus_preference_set_description                (NautilusPreference       *preference,
									   const char               *description);
/* Methods to deal with enum preferences */
void                   nautilus_preference_enum_add_entry                 (NautilusPreference       *preference,
									   const char               *entry_name,
									   const char               *entry_description,
									   gint                      entry_value);
char *                 nautilus_preference_enum_get_nth_entry_name        (const NautilusPreference *preference,
									   guint                     n);
char *                 nautilus_preference_enum_get_nth_entry_description (const NautilusPreference *preference,
									   guint                     n);
gint                   nautilus_preference_enum_get_nth_entry_value       (const NautilusPreference *preference,
									   guint                     n);
guint                  nautilus_preference_enum_get_num_entries           (const NautilusPreference *preference);


/*
 *
 */
NautilusPreference *nautilus_preference_find_by_name           (const char             *name);
void                nautilus_preference_set_info_by_name       (const char             *name,
								const char             *description,
								NautilusPreferenceType  type,
								gconstpointer		*default_values,
								guint			num_default_values);
void                nautilus_preference_enum_add_entry_by_name (const char             *name,
								const char             *entry_name,
								const char             *entry_description,
								int                     entry_value);

void nautilus_preference_shutdown (void);


BEGIN_GNOME_DECLS

#endif /* NAUTILUS_PREFERENCE_H */
