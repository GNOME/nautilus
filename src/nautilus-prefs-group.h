/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-prefs-group.h - Interface for a prefs group superclass.

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

#ifndef NAUTILUS_PREFS_GROUP_H
#define NAUTILUS_PREFS_GROUP_H

#include <libgnomeui/gnome-dialog.h>
#include <gtk/gtkframe.h>

//#include <gnome.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_PREFS_GROUP            (nautilus_prefs_group_get_type ())
#define NAUTILUS_PREFS_GROUP(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_PREFS_GROUP, NautilusPrefsGroup))
#define NAUTILUS_PREFS_GROUP_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PREFS_GROUP, NautilusPrefsGroupClass))
#define NAUTILUS_IS_PREFS_GROUP(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_PREFS_GROUP))
#define NAUTILUS_IS_PREFS_GROUP_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PREFS_GROUP))
#define NAUTILUS_PREFS_GROUP_ASSERT_METHOD(group, method) \
NAUTILUS_ASSERT_METHOD (group, NAUTILUS_TYPE_PREFS_GROUP, NautilusPrefsGroupClass, method)
#define NAUTILUS_PREFS_GROUP_INVOKE_METHOD(group, method) \
NAUTILUS_INVOKE_METHOD (group, NAUTILUS_TYPE_PREFS_GROUP, NautilusPrefsGroupClass, method)

typedef struct _NautilusPrefsGroup	     NautilusPrefsGroup;
typedef struct _NautilusPrefsGroupClass      NautilusPrefsGroupClass;
typedef struct _NautilusPrefsGroupPrivate    NautilusPrefsGroupPrivate;

struct _NautilusPrefsGroup
{
	/* Super Class */
	GtkFrame			frame;

	/* Private stuff */
	NautilusPrefsGroupPrivate	*priv;
};

struct _NautilusPrefsGroupClass
{
	GtkFrameClass	parent_class;

	void (*construct) (NautilusPrefsGroup *prefs_group, const gchar *group_title);

	void (*changed) (NautilusPrefsGroup *prefs_group);
};

GtkType    nautilus_prefs_group_get_type              (void);
// GtkWidget* nautilus_prefs_group_new                   (const gchar *group_title);
void  nautilus_prefs_group_set_title (NautilusPrefsGroup *prefs_group,
				      const gchar *group_title);
GtkWidget*  nautilus_prefs_group_get_content_box (NautilusPrefsGroup *prefs_group);

BEGIN_GNOME_DECLS

#endif /* NAUTILUS_PREFS_GROUP_H */


