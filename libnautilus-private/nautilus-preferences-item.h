/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-preferences-item.h - Interface for an individual prefs item.

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

#ifndef NAUTILUS_PREFERENCES_ITEM_H
#define NAUTILUS_PREFERENCES_ITEM_H

#include <gtk/gtkvbox.h>
#include <libnautilus-extensions/nautilus-preferences.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_PREFERENCES_ITEM            (nautilus_preferences_item_get_type ())
#define NAUTILUS_PREFERENCES_ITEM(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_PREFERENCES_ITEM, NautilusPreferencesItem))
#define NAUTILUS_PREFERENCES_ITEM_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PREFERENCES_ITEM, NautilusPreferencesItemClass))
#define NAUTILUS_IS_PREFERENCES_ITEM(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_PREFERENCES_ITEM))
#define NAUTILUS_IS_PREFERENCES_ITEM_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PREFERENCES_ITEM))

typedef struct _NautilusPreferencesItem		  NautilusPreferencesItem;
typedef struct _NautilusPreferencesItemClass      NautilusPreferencesItemClass;
typedef struct _NautilusPreferencesItemDetails    NautilusPreferencesItemDetails;

struct _NautilusPreferencesItem
{
	/* Super Class */
	GtkVBox				vbox;

	/* Private stuff */
	NautilusPreferencesItemDetails	*details;
};

struct _NautilusPreferencesItemClass
{
	GtkVBoxClass	vbox_class;
};

/*
 * NautilusPreferencesItemType:
 *
 * The types of supported preferences that also have a corresponding ui in the 
 * preferences dialog.  Note that this is different than NautilusPreferencesType
 * because it is possible to have a prefernce that is not exposed in the ui.
 */
typedef enum
{
	NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	NAUTILUS_PREFERENCE_ITEM_ENUM,
	NAUTILUS_PREFERENCE_ITEM_SHORT_ENUM,
	NAUTILUS_PREFERENCE_ITEM_FONT,
	NAUTILUS_PREFERENCE_ITEM_SMOOTH_FONT,
	NAUTILUS_PREFERENCE_ITEM_EDITABLE_STRING,
	NAUTILUS_PREFERENCE_ITEM_EDITABLE_INTEGER,
	NAUTILUS_PREFERENCE_ITEM_CONSTRAINED_INTEGER
} NautilusPreferencesItemType;

typedef enum
{
	NAUTILUS_PREFERENCE_ITEM_SHOW,
	NAUTILUS_PREFERENCE_ITEM_HIDE
} NautilusPreferencesItemControlAction;

GtkType    nautilus_preferences_item_get_type                       (void);
GtkWidget* nautilus_preferences_item_new                            (const char                           *preference_name,
								     NautilusPreferencesItemType           item_type);
char *     nautilus_preferences_item_get_name                       (const NautilusPreferencesItem        *preferences_item);
void       nautilus_preferences_item_set_control_preference         (NautilusPreferencesItem              *preferences_item,
								     const char                           *control_preference_name);
void       nautilus_preferences_item_set_control_action             (NautilusPreferencesItem              *preferences_item,
								     NautilusPreferencesItemControlAction  control_action);
gboolean   nautilus_preferences_item_get_control_showing            (const NautilusPreferencesItem        *preferences_item);
void       nautilus_preferences_item_set_constrained_integer_values (NautilusPreferencesItem              *preferences_item,
								     const char                           *values,
								     const char                           *labels);
gboolean   nautilus_preferences_item_child_is_caption               (const NautilusPreferencesItem        *preferences_item);
int        nautilus_preferences_item_get_caption_title_label_width  (const NautilusPreferencesItem        *item);
void       nautilus_preferences_item_set_caption_spacing            (NautilusPreferencesItem              *item,
								     int                                   spacing);
void       nautilus_preferences_item_update_showing                 (NautilusPreferencesItem              *item);

END_GNOME_DECLS

#endif /* NAUTILUS_PREFERENCES_ITEM_H */


