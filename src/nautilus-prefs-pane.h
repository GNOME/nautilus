/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-prefs-pane.h - Interface for a prefs pane superclass.

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

#ifndef NAUTILUS_PREFS_PANE_H
#define NAUTILUS_PREFS_PANE_H

#include <libgnomeui/gnome-dialog.h>
#include <gtk/gtkvbox.h>
#include "nautilus-prefs-group.h"

//#include <gnome.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_PREFS_PANE            (nautilus_prefs_pane_get_type ())
#define NAUTILUS_PREFS_PANE(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_PREFS_PANE, NautilusPrefsPane))
#define NAUTILUS_PREFS_PANE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PREFS_PANE, NautilusPrefsPaneClass))
#define NAUTILUS_IS_PREFS_PANE(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_PREFS_PANE))
#define NAUTILUS_IS_PREFS_PANE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PREFS_PANE))

typedef struct _NautilusPrefsPane	   NautilusPrefsPane;
typedef struct _NautilusPrefsPaneClass      NautilusPrefsPaneClass;
typedef struct _NautilusPrefsPanePrivate    NautilusPrefsPanePrivate;

struct _NautilusPrefsPane
{
	/* Super Class */
	GtkVBox				vbox;

	/* Private stuff */
	NautilusPrefsPanePrivate	*priv;
};

struct _NautilusPrefsPaneClass
{
	GtkVBoxClass	parent_class;

	void (*construct) (NautilusPrefsPane *prefs_pane, GtkWidget *box);
};

GtkType    nautilus_prefs_pane_get_type              (void);
GtkWidget* nautilus_prefs_pane_new                   (const gchar *pane_title,
						      const gchar *pane_description);
void nautilus_prefs_pane_set_title                   (NautilusPrefsPane * prefs_pane,
						      const gchar *pane_title);
void nautilus_prefs_pane_set_description             (NautilusPrefsPane * prefs_pane,
						      const gchar *pane_description);
void nautilus_prefs_pane_add_group		     (NautilusPrefsPane *prefs_pane,
						      GtkWidget *prefs_group);

BEGIN_GNOME_DECLS

#endif /* NAUTILUS_PREFS_PANE_H */
