/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-preferences-group.h - A group of preferences items bounded by a frame.

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

#ifndef NAUTILUS_PREFERENCES_GROUP_H
#define NAUTILUS_PREFERENCES_GROUP_H

#include <gtk/gtkframe.h>

#include <libnautilus-extensions/nautilus-preferences.h>
#include <libnautilus-extensions/nautilus-preferences-item.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_PREFERENCES_GROUP            (nautilus_preferences_group_get_type ())
#define NAUTILUS_PREFERENCES_GROUP(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_PREFERENCES_GROUP, NautilusPreferencesGroup))
#define NAUTILUS_PREFERENCES_GROUP_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PREFERENCES_GROUP, NautilusPreferencesGroupClass))
#define NAUTILUS_IS_PREFERENCES_GROUP(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_PREFERENCES_GROUP))
#define NAUTILUS_IS_PREFERENCES_GROUP_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PREFERENCES_GROUP))

typedef struct NautilusPreferencesGroup		   NautilusPreferencesGroup;
typedef struct NautilusPreferencesGroupClass	   NautilusPreferencesGroupClass;
typedef struct NautilusPreferencesGroupDetails	   NautilusPreferencesGroupDetails;

struct NautilusPreferencesGroup
{
	/* Super Class */
	GtkFrame frame;
	
	/* Private stuff */
	NautilusPreferencesGroupDetails	*details;
};

struct NautilusPreferencesGroupClass
{
	GtkFrameClass parent_class;
};

GtkType    nautilus_preferences_group_get_type              (void);
GtkWidget* nautilus_preferences_group_new                   (const gchar                    *title);
GtkWidget* nautilus_preferences_group_add_item              (NautilusPreferencesGroup       *group,
							     const char                     *preference_name,
							     NautilusPreferencesItemType     item_type,
							     int                             column);
void       nautilus_preferences_group_update                (NautilusPreferencesGroup       *group);
guint      nautilus_preferences_group_get_num_visible_items (const NautilusPreferencesGroup *group);
char *     nautilus_preferences_group_get_title_label       (const NautilusPreferencesGroup *group);
int        nautilus_preferences_group_get_max_caption_width (const NautilusPreferencesGroup *group,
							     int                             column);
void       nautilus_preferences_group_align_captions        (NautilusPreferencesGroup       *group,
							     int                             max_caption_width,
							     int                             column);

END_GNOME_DECLS

#endif /* NAUTILUS_PREFERENCES_GROUP_H */


