/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-radio-button-group.h - A radio button group container.

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

#ifndef NAUTILUS_RADIO_BUTTON_GROUP_H
#define NAUTILUS_RADIO_BUTTON_GROUP_H

#include <gtk/gtktable.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gnome.h>

/*
 * NautilusRadioButtonGroup is a collection of radio buttons
 * arranged into rows.  An optional icon and description can 
 * can be displayed with each radio button entry:
 * 
 * Icon1 < > Item one           Item 1 description
 * Icon2 < > Item two           Item 2 description
 * Icon3 <x> Item three         Item 3 description
 * Icon4 < > Item four          Item 4 description
 * 
 */
BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_RADIO_BUTTON_GROUP            (nautilus_radio_button_group_get_type ())
#define NAUTILUS_RADIO_BUTTON_GROUP(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_RADIO_BUTTON_GROUP, NautilusRadioButtonGroup))
#define NAUTILUS_RADIO_BUTTON_GROUP_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_RADIO_BUTTON_GROUP, NautilusRadioButtonGroupClass))
#define NAUTILUS_IS_RADIO_BUTTON_GROUP(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_RADIO_BUTTON_GROUP))

typedef struct _NautilusRadioButtonGroup	   NautilusRadioButtonGroup;
typedef struct _NautilusRadioButtonGroupClass      NautilusRadioButtonGroupClass;
typedef struct _NautilusRadioButtonGroupDetails    NautilusRadioButtonGroupDetails;

struct _NautilusRadioButtonGroup
{
	/* Super Class */
	GtkTable			table;
	
	/* Private stuff */
	NautilusRadioButtonGroupDetails	*details;
};

struct _NautilusRadioButtonGroupClass
{
	GtkTableClass			parent_class;
};

typedef struct
{
	guint active_button_index;
} NautilusRadioButtonGroupSignalData;

GtkType    nautilus_radio_button_group_get_type                   (void);
GtkWidget* nautilus_radio_button_group_new	                  (gboolean is_horizontal);


/* Insert a new item to the group.  Returns the new item's index */
guint      nautilus_radio_button_group_insert                     (NautilusRadioButtonGroup *button_group,
								   const gchar              *label);

/* Get the active item index. By law there always is an active item */
guint      nautilus_radio_button_group_get_active_index           (NautilusRadioButtonGroup *button_group);

/* Set the active item index. */
void       nautilus_radio_button_group_set_active_index           (NautilusRadioButtonGroup *button_group,
								   guint                     active_index);

/* Set an item's pixbuf. */
void       nautilus_radio_button_group_set_entry_pixbuf           (NautilusRadioButtonGroup *button_group,
								   guint                     entry_index,
								   GdkPixbuf                *pixbuf);
/* Set an item's description. */
void       nautilus_radio_button_group_set_entry_description_text (NautilusRadioButtonGroup *button_group,
								   guint                     entry_index,
								   const char               *description);

BEGIN_GNOME_DECLS

#endif /* NAUTILUS_RADIO_BUTTON_GROUP_H */


