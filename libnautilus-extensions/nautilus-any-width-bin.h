/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-any-width-bin.h:  Subclass of NautilusGenerousBin that doesn't
 			      specify a width (so it won't cause its parent to widen)

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

   Author: John Sullivan <sullivan@eazel.com>
 */

#ifndef NAUTILUS_ANY_WIDTH_BIN_H
#define NAUTILUS_ANY_WIDTH_BIN_H

#include "nautilus-generous-bin.h"

#define NAUTILUS_TYPE_ANY_WIDTH_BIN            (nautilus_any_width_bin_get_type ())
#define NAUTILUS_ANY_WIDTH_BIN(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_ANY_WIDTH_BIN, NautilusAnyWidthBin))
#define NAUTILUS_ANY_WIDTH_BIN_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ANY_WIDTH_BIN, NautilusAnyWidthBinClass))
#define NAUTILUS_IS_ANY_WIDTH_BIN(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_ANY_WIDTH_BIN))
#define NAUTILUS_IS_ANY_WIDTH_BIN_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_ANY_WIDTH_BIN))

typedef struct NautilusAnyWidthBin NautilusAnyWidthBin;
typedef struct NautilusAnyWidthBinClass NautilusAnyWidthBinClass;

struct NautilusAnyWidthBin {
	NautilusGenerousBin parent_slot;
};

struct NautilusAnyWidthBinClass {
	NautilusGenerousBinClass parent_slot;
};

GtkType    nautilus_any_width_bin_get_type (void);
GtkWidget *nautilus_any_width_bin_new (void);

#endif /* NAUTILUS_ANY_WIDTH_BIN_H */
