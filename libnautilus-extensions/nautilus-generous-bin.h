/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-generous-bin.h: Subclass of GtkBin that gives all of its
                            allocation to its child.

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

   Author: Darin Adler <darin@eazel.com>
 */

#ifndef NAUTILUS_GENEROUS_BIN_H
#define NAUTILUS_GENEROUS_BIN_H

#include <gtk/gtkbin.h>

#define NAUTILUS_TYPE_GENEROUS_BIN            (nautilus_generous_bin_get_type ())
#define NAUTILUS_GENEROUS_BIN(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_GENEROUS_BIN, NautilusGenerousBin))
#define NAUTILUS_GENEROUS_BIN_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_GENEROUS_BIN, NautilusGenerousBinClass))
#define NAUTILUS_IS_GENEROUS_BIN(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_GENEROUS_BIN))
#define NAUTILUS_IS_GENEROUS_BIN_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_GENEROUS_BIN))

typedef struct NautilusGenerousBin NautilusGenerousBin;
typedef struct NautilusGenerousBinClass NautilusGenerousBinClass;

struct NautilusGenerousBin {
	GtkBin parent_slot;
};

struct NautilusGenerousBinClass {
	GtkBinClass parent_slot;
};

GtkType nautilus_generous_bin_get_type (void);

#endif /* NAUTILUS_GENEROUS_BIN_H */
