/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-horizontal-splitter.h - A horizontal splitter with a semi gradient look

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

#ifndef NAUTILUS_HORIZONTAL_SPLITTER_H
#define NAUTILUS_HORIZONTAL_SPLITTER_H

#include <libgnome/gnome-defs.h>
#include "e-hpaned.h"

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_HORIZONTAL_SPLITTER            (nautilus_horizontal_splitter_get_type ())
#define NAUTILUS_HORIZONTAL_SPLITTER(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_HORIZONTAL_SPLITTER, NautilusHorizontalSplitter))
#define NAUTILUS_HORIZONTAL_SPLITTER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_HORIZONTAL_SPLITTER, NautilusHorizontalSplitterClass))
#define NAUTILUS_IS_HORIZONTAL_SPLITTER(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_HORIZONTAL_SPLITTER))
#define NAUTILUS_IS_HORIZONTAL_SPLITTER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_HORIZONTAL_SPLITTER))

typedef struct NautilusHorizontalSplitterDetails NautilusHorizontalSplitterDetails;

typedef struct {
	EHPaned					parent_slot;
	NautilusHorizontalSplitterDetails	*details;
} NautilusHorizontalSplitter;

typedef struct {
	EHPanedClass				parent_slot;
} NautilusHorizontalSplitterClass;

/* NautilusHorizontalSplitter public methods */
GtkType    nautilus_horizontal_splitter_get_type (void);
GtkWidget *nautilus_horizontal_splitter_new      (void);

void	   nautilus_horizontal_splitter_collapse	(NautilusHorizontalSplitter *splitter);
void	   nautilus_horizontal_splitter_expand		(NautilusHorizontalSplitter *splitter);
void	   nautilus_horizontal_splitter_toggle_position	(NautilusHorizontalSplitter *splitter);

END_GNOME_DECLS

#endif /* NAUTILUS_HORIZONTAL_SPLITTER_H */
