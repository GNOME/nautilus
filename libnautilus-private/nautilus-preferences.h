/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-prefs.h - Preference peek/poke/notify object interface.

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

#ifndef NAUTILUS_PREFERENCES_H
#define NAUTILUS_PREFERENCES_H

#include <gtk/gtkhbox.h>
#include <nautilus-widgets/nautilus-preferences-pane.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_PREFS            (nautilus_preferences_get_type ())
#define NAUTILUS_PREFS(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_PREFS, NautilusPreferences))
#define NAUTILUS_PREFERENCES_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PREFS, NautilusPreferencesClass))
#define NAUTILUS_IS_PREFS(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_PREFS))
#define NAUTILUS_IS_PREFS_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PREFS))

typedef struct _NautilusPreferences	     NautilusPreferences;
typedef struct _NautilusPreferencesClass     NautilusPreferencesClass;
typedef struct _NautilusPreferencesDetails   NautilusPreferencesDetails;

struct _NautilusPreferences
{
	/* Super Class */
	GtkObject			object;

	/* Private stuff */
	NautilusPreferencesDetails	*details;
};

/*
 * The types of supported preferences.
 */
typedef enum
{
	NAUTILUS_PREFERENCE_BOOLEAN,
	NAUTILUS_PREFERENCE_ENUM,
	NAUTILUS_PREFERENCE_STRING
} NautilusPreferencesType;

/*
 * A callback which you can register to to be notified when a particular
 * preference changes.
 */
typedef void (*NautilusPreferencesCallback) (const GtkObject		*prefs,
					     const gchar		*pref_name, 
					     NautilusPreferencesType	pref_type,
					     gconstpointer		pref_value,
					     gpointer			user_data);

struct _NautilusPreferencesClass
{
	GtkObjectClass	object_class;
};

typedef struct
{
	gchar			*pref_name;
	gchar			*pref_description;
	NautilusPreferencesType	pref_type;
	gconstpointer		pref_default_value;
	gpointer		type_data;
} NautilusPreferencesInfo;

typedef struct
{
	const gchar	**enum_names;
	const gchar	**enum_descriptions;
	const gint	*enum_values;
	guint		num_entries;
} NautilusPreferencesEnumData;

GtkType                        nautilus_preferences_get_type               (void);
GtkObject*                     nautilus_preferences_new                    (const gchar                   *domain);
void                           nautilus_preferences_register_from_info     (NautilusPreferences           *prefs,
									    const NautilusPreferencesInfo *pref_info);
void                           nautilus_preferences_register_from_values   (NautilusPreferences           *prefs,
									    gchar                         *pref_name,
									    gchar                         *pref_description,
									    NautilusPreferencesType        pref_type,
									    gconstpointer                  pref_default_value,
									    gpointer                       type_data);
const NautilusPreferencesInfo *nautilus_preferences_get_pref_info          (NautilusPreferences           *prefs,
									    const gchar                   *pref_name);
gboolean                       nautilus_preferences_add_callback           (NautilusPreferences           *prefs,
									    const gchar                   *pref_name,
									    NautilusPreferencesCallback    callback,
									    gpointer                       user_data);
gboolean                       nautilus_preferences_remove_callback        (NautilusPreferences           *prefs,
									    const gchar                   *pref_name,
									    NautilusPreferencesCallback    callback,
									    gpointer                       user_data);
void                           nautilus_preferences_set_boolean            (NautilusPreferences           *prefs,
									    const gchar                   *pref_name,
									    gboolean                       boolean_value);
gboolean                       nautilus_preferences_get_boolean            (NautilusPreferences           *prefs,
									    const gchar                   *pref_name);
void                           nautilus_preferences_set_enum               (NautilusPreferences           *prefs,
									    const gchar                   *pref_name,
									    gint                           enum_value);
gint                           nautilus_preferences_get_enum               (NautilusPreferences           *prefs,
									    const gchar                   *pref_name);
void                           nautilus_preferences_set_string             (NautilusPreferences           *prefs,
									    const gchar                   *pref_name,
									    const char                    *string_value);
char *                         nautilus_preferences_get_string             (NautilusPreferences           *prefs,
									    const gchar                   *pref_name);
NautilusPreferences *          nautilus_preferences_get_global_preferences (void);


BEGIN_GNOME_DECLS

#endif /* NAUTILUS_PREFERENCES_H */


