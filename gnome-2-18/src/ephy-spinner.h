/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright © 2000 Eazel, Inc.
 * Copyright © 2004, 2006 Christian Persch
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
 * $Id$
 */

#ifndef EPHY_SPINNER_H
#define EPHY_SPINNER_H

#include <gtk/gtkwidget.h>
#include <gtk/gtkenums.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SPINNER		(ephy_spinner_get_type ())
#define EPHY_SPINNER(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_SPINNER, EphySpinner))
#define EPHY_SPINNER_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_SPINNER, EphySpinnerClass))
#define EPHY_IS_SPINNER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_SPINNER))
#define EPHY_IS_SPINNER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_SPINNER))
#define EPHY_SPINNER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_SPINNER, EphySpinnerClass))

typedef struct _EphySpinner		EphySpinner;
typedef struct _EphySpinnerClass	EphySpinnerClass;
typedef struct _EphySpinnerDetails	EphySpinnerDetails;

struct _EphySpinner
{
	GtkWidget parent;

	/*< private >*/
	EphySpinnerDetails *details;
};

struct _EphySpinnerClass
{
	GtkWidgetClass parent_class;
};

GType		ephy_spinner_get_type	(void);

GtkWidget      *ephy_spinner_new	(void);

void		ephy_spinner_start	(EphySpinner *throbber);

void		ephy_spinner_stop	(EphySpinner *throbber);

void		ephy_spinner_set_size	(EphySpinner *spinner,
					 GtkIconSize size);

G_END_DECLS

#endif /* EPHY_SPINNER_H */
