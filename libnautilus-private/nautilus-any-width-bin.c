/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-any-width-bin.c:  Subclass of NautilusGenerousBin that doesn't
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

   Author: John Sullivan <sullivan@eazel.com>,
 */

#include <config.h>
#include "nautilus-any-width-bin.h"

#include "nautilus-gtk-macros.h"

static void nautilus_any_width_bin_initialize_class (NautilusAnyWidthBinClass *class);
static void nautilus_any_width_bin_initialize       (NautilusAnyWidthBin      *box);
static void nautilus_any_width_bin_size_request     (GtkWidget                *widget,
						     GtkRequisition	      *requisition);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusAnyWidthBin, nautilus_any_width_bin, NAUTILUS_TYPE_GENEROUS_BIN)

static void
nautilus_any_width_bin_initialize_class (NautilusAnyWidthBinClass *klass)
{
	GTK_WIDGET_CLASS (klass)->size_request = nautilus_any_width_bin_size_request;
}

static void
nautilus_any_width_bin_initialize (NautilusAnyWidthBin *bin)
{
}

GtkWidget*
nautilus_any_width_bin_new (void)
{
	NautilusAnyWidthBin *bin;

	bin = gtk_type_new (nautilus_any_width_bin_get_type ());
	
	return GTK_WIDGET (bin);
}

static void
nautilus_any_width_bin_size_request (GtkWidget *widget,
				       GtkRequisition *requisition)
{
	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, size_request, (widget, requisition));

	/* Don't specify a particular width; we'll take whatever we get. */
	requisition->width = 0;
}
