/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-ellipsizing-label.h: Subclass of GtkLabel that ellipsizes the text.

   Copyright (C) 2001 Eazel, Inc.

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

#ifndef NAUTILUS_ELLIPSIZING_LABEL_H
#define NAUTILUS_ELLIPSIZING_LABEL_H

#include <gtk/gtklabel.h>

#define NAUTILUS_TYPE_ELLIPSIZING_LABEL            (nautilus_ellipsizing_label_get_type ())
#define NAUTILUS_ELLIPSIZING_LABEL(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_ELLIPSIZING_LABEL, NautilusEllipsizingLabel))
#define NAUTILUS_ELLIPSIZING_LABEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ELLIPSIZING_LABEL, NautilusEllipsizingLabelClass))
#define NAUTILUS_IS_ELLIPSIZING_LABEL(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_ELLIPSIZING_LABEL))
#define NAUTILUS_IS_ELLIPSIZING_LABEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_ELLIPSIZING_LABEL))

typedef struct NautilusEllipsizingLabel NautilusEllipsizingLabel;
typedef struct NautilusEllipsizingLabelClass NautilusEllipsizingLabelClass;

typedef struct NautilusEllipsizingLabelDetails NautilusEllipsizingLabelDetails;

struct NautilusEllipsizingLabel {
	GtkLabel parent;
	NautilusEllipsizingLabelDetails *details;
};

struct NautilusEllipsizingLabelClass {
	GtkLabelClass parent_class;
};

GtkType    nautilus_ellipsizing_label_get_type  (void);
GtkWidget *nautilus_ellipsizing_label_new       (const char 		  *string);
void	   nautilus_ellipsizing_label_set_text  (NautilusEllipsizingLabel *label,
						 const char 		  *string);

#endif /* NAUTILUS_ELLIPSIZING_LABEL_H */
