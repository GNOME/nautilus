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

#ifndef NAUTILUS_PREFS_H
#define NAUTILUS_PREFS_H

#include <gtk/gtkhbox.h>
#include <nautilus-widgets/nautilus-preferences-pane.h>

//#include <gnome.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_PREFS            (nautilus_prefs_get_type ())
#define NAUTILUS_PREFS(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_PREFS, NautilusPrefs))
#define NAUTILUS_PREFS_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PREFS, NautilusPrefsClass))
#define NAUTILUS_IS_PREFS(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_PREFS))
#define NAUTILUS_IS_PREFS_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PREFS))

typedef struct _NautilusPrefs	       NautilusPrefs;
typedef struct _NautilusPrefsClass     NautilusPrefsClass;
typedef struct _NautilusPrefsPrivate   NautilusPrefsPrivate;

struct _NautilusPrefs
{
	/* Super Class */
	GtkObject		object;

	/* Private stuff */
	NautilusPrefsPrivate	*priv;
};

typedef void (*NautilusPrefsCallback) (const GtkObject		*prefs,
				       const gchar		*pref_name, 
				       GtkFundamentalType	pref_type,
				       gconstpointer		pref_value,
				       gpointer			user_data);

struct _NautilusPrefsClass
{
	GtkObjectClass	object_class;

//	void (*activate) (GtkWidget * prefs, gint entry_number);
};

typedef struct
{
	gchar			*pref_name;
	gchar			*pref_description;
	GtkFundamentalType	pref_type;
	gconstpointer		pref_default_value;
	gpointer		type_data;
} NautilusPrefInfo;

typedef struct
{
	const gchar	**enum_names;
	const gchar	**enum_descriptions;
	const gint	*enum_values;
	guint		num_entries;
} NautilusPrefEnumData;

GtkType                 nautilus_prefs_get_type             (void);
GtkObject*              nautilus_prefs_new                  (const gchar            *domain);
void                    nautilus_prefs_register_from_info   (NautilusPrefs          *prefs,
							     const NautilusPrefInfo *pref_info);
void                    nautilus_prefs_register_from_values (NautilusPrefs          *prefs,
							     gchar                  *pref_name,
							     gchar                  *pref_description,
							     GtkFundamentalType      pref_type,
							     gconstpointer           pref_default_value,
							     gpointer                type_data);
const NautilusPrefInfo *nautilus_prefs_get_pref_info        (NautilusPrefs          *prefs,
							     const gchar            *pref_name);
gboolean                nautilus_prefs_add_callback         (NautilusPrefs          *prefs,
							     const gchar            *pref_name,
							     NautilusPrefsCallback   callback,
							     gpointer                user_data);
void                    nautilus_prefs_set_boolean          (NautilusPrefs          *prefs,
							     const gchar            *pref_name,
							     gboolean                boolean_value);
gboolean                nautilus_prefs_get_boolean          (NautilusPrefs          *prefs,
							     const gchar            *pref_name);
void                    nautilus_prefs_set_enum             (NautilusPrefs          *prefs,
							     const gchar            *pref_name,
							     gint                    enum_value);
gint                    nautilus_prefs_get_enum             (NautilusPrefs          *prefs,
							     const gchar            *pref_name);


BEGIN_GNOME_DECLS

#endif /* NAUTILUS_PREFS_H */


