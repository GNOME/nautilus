/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: J Shane Culpepper
 */

#ifndef NAUTILUS_LOGIN_VIEW_H
#define NAUTILUS_LOGIN_VIEW_H

#include <libnautilus/nautilus-view.h>
#include <gtk/gtk.h>

typedef struct _NautilusLoginView NautilusLoginView;
typedef struct _NautilusLoginViewClass NautilusLoginViewClass;

#define NAUTILUS_TYPE_LOGIN_VIEW		(nautilus_login_view_get_type ())
#define NAUTILUS_LOGIN_VIEW(obj)		(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_LOGIN_VIEW, NautilusLoginView))
#define NAUTILUS_LOGIN_VIEW_CLASS (klass)	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_LOGIN_VIEW, NautilusLoginViewClass))
#define NAUTILUS_IS_LOGIN_VIEW(obj)		(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_LOGIN_VIEW))
#define NAUTILUS_IS_LOGIN_VIEW_CLASS (klass)	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_LOGIN_VIEW))

typedef struct _NautilusLoginViewDetails NautilusLoginViewDetails;

struct _NautilusLoginView {
        GtkEventBox parent;
        NautilusLoginViewDetails *details;
};

struct _NautilusLoginViewClass {
        GtkVBoxClass parent_class;
};

/* GtkObject support */
GtkType		nautilus_login_view_get_type			(void);

/* Component embedding support */
NautilusView 	*nautilus_login_view_get_nautilus_view	(NautilusLoginView *view);

/* URI handling */
void		nautilus_login_view_load_uri		(NautilusLoginView	*view,
							 const char		*uri);

#endif /* NAUTILUS_LOGIN_VIEW_H */

