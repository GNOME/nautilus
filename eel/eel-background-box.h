/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-background-box.c - an event box that renders an eel background

   Copyright (C) 2002 Sun Microsystems, Inc.

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

   Author: Dave Camp <dave@ximian.com>
*/

#ifndef EEL_BACKGROUND_BOX_H
#define EEL_BACKGROUND_BOX_H

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EEL_TYPE_BACKGROUND_BOX            (eel_background_box_get_type ())
#define EEL_BACKGROUND_BOX(obj)            (GTK_CHECK_CAST ((obj), EEL_TYPE_BACKGROUND_BOX, EelBackgroundBox))
#define EEL_BACKGROUND_BOX_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), EEL_TYPE_BACKGROUND_BOX, EelBackgroundBoxClass))
#define EEL_IS_BACKGROUND_BOX(obj)         (GTK_CHECK_TYPE ((obj), EEL_TYPE_BACKGROUND_BOX))
#define EEL_IS_BACKGROUND_BOX_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), EEL_TYPE_BACKGROUND_BOX))

typedef struct EelBackgroundBox	       EelBackgroundBox;
typedef struct EelBackgroundBoxClass       EelBackgroundBoxClass;
typedef struct EelBackgroundBoxDetails     EelBackgroundBoxDetails;

struct EelBackgroundBox
{
	/* Superclass */
	GtkEventBox event_box;
};

struct EelBackgroundBoxClass 
{
	GtkEventBoxClass parent_class;
};

GtkType    eel_background_box_get_type (void);
GtkWidget *eel_background_box_new      (void);

G_END_DECLS

#endif /* EEL_BACKGROUND_TABLE_H */


