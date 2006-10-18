/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 *
 * This is the header file for the throbber on the location bar
 *
 */

#ifndef NAUTILUS_THROBBER_H
#define NAUTILUS_THROBBER_H

#include "ephy-spinner.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_THROBBER		(nautilus_throbber_get_type ())
#define NAUTILUS_THROBBER(obj)		(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_THROBBER, NautilusThrobber))
#define NAUTILUS_THROBBER_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_THROBBER, NautilusThrobberClass))
#define NAUTILUS_IS_THROBBER(obj)	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_THROBBER))
#define NAUTILUS_IS_THROBBER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_THROBBER))

typedef EphySpinner NautilusThrobber;
typedef EphySpinnerClass NautilusThrobberClass;

GType         nautilus_throbber_get_type       (void);
GtkWidget    *nautilus_throbber_new            (void);
void          nautilus_throbber_start          (NautilusThrobber *throbber);
void          nautilus_throbber_stop           (NautilusThrobber *throbber);
void          nautilus_throbber_set_size       (NautilusThrobber *throbber, GtkIconSize size);

G_END_DECLS

#endif /* NAUTILUS_THROBBER_H */


