/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-keep-last-vertical-box.h: Subclass of GtkVBox that clips off
 				      items that don't fit, except the last one.

   Copyright (C) 2000 Eazel, Inc.

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

   Author: John Sullivan <sullivan@eazel.com>,
 */

#ifndef NAUTILUS_KEEP_LAST_VERTICAL_BOX_H
#define NAUTILUS_KEEP_LAST_VERTICAL_BOX_H

#include <gtk/gtkvbox.h>

#define NAUTILUS_TYPE_KEEP_LAST_VERTICAL_BOX            (nautilus_keep_last_vertical_box_get_type ())
#define NAUTILUS_KEEP_LAST_VERTICAL_BOX(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_KEEP_LAST_VERTICAL_BOX, NautilusKeepLastVerticalBox))
#define NAUTILUS_KEEP_LAST_VERTICAL_BOX_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_KEEP_LAST_VERTICAL_BOX, NautilusKeepLastVerticalBoxClass))
#define NAUTILUS_IS_KEEP_LAST_VERTICAL_BOX(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_KEEP_LAST_VERTICAL_BOX))
#define NAUTILUS_IS_KEEP_LAST_VERTICAL_BOX_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_KEEP_LAST_VERTICAL_BOX))

typedef struct NautilusKeepLastVerticalBox NautilusKeepLastVerticalBox;
typedef struct NautilusKeepLastVerticalBoxClass NautilusKeepLastVerticalBoxClass;

struct NautilusKeepLastVerticalBox {
	GtkVBox vbox;
};

struct NautilusKeepLastVerticalBoxClass {
	GtkVBoxClass parent_class;
};

GType      nautilus_keep_last_vertical_box_get_type  (void);
GtkWidget *nautilus_keep_last_vertical_box_new       (gint spacing);

#endif /* NAUTILUS_KEEP_LAST_VERTICAL_BOX_H */
