/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-toolbar.h - Toolbar for Nautilus that overcomes fixed spacing problem

   Copyright (C) 2000 Eazel, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; see the file COPYING.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Andy Hertzfeld <andy@eazel.com>
*/

#ifndef NAUTILUS_TOOLBAR_H
#define NAUTILUS_TOOLBAR_H

#include <gtk/gtktoolbar.h>

#define NAUTILUS_TYPE_TOOLBAR (nautilus_toolbar_get_type ())
#define NAUTILUS_TOOLBAR(obj) \
	GTK_CHECK_CAST (obj, NAUTILUS_TYPE_TOOLBAR, NautilusToolbar)
#define NAUTILUS_TOOLBAR_CLASS(klass) \
	GTK_CHECK_CLASS_CAST (klass, NAUTILUS_TYPE_TOOLBAR, NautilusToolbarClass)
#define NAUTILUS_IS_TOOLBAR(obj) \
	GTK_CHECK_TYPE (obj, NAUTILUS_TYPE_TOOLBAR)

typedef struct NautilusToolbar {
	GtkToolbar parent;
	int button_spacing;
} NautilusToolbar;

typedef struct {
	GtkToolbarClass parent_class;
} NautilusToolbarClass;

GtkType    nautilus_toolbar_get_type     	(void);
GtkWidget* nautilus_toolbar_new			(void);
void	   nautilus_toolbar_set_button_spacing  (NautilusToolbar *toolbar, int spacing);

#endif /* NAUTILUS_TOOLBAR_H */
