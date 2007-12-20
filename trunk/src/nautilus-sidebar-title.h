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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 */

/*
 * This is the header file for the sidebar title, which is part of the sidebar.
 */

#ifndef NAUTILUS_SIDEBAR_TITLE_H
#define NAUTILUS_SIDEBAR_TITLE_H

#include <gtk/gtkvbox.h>
#include <eel/eel-background.h>
#include <libnautilus-private/nautilus-file.h>

#define NAUTILUS_TYPE_SIDEBAR_TITLE	       (nautilus_sidebar_title_get_type ())
#define NAUTILUS_SIDEBAR_TITLE(obj)	       (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_SIDEBAR_TITLE, NautilusSidebarTitle))
#define NAUTILUS_SIDEBAR_TITLE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SIDEBAR_TITLE, NautilusSidebarTitleClass))
#define NAUTILUS_IS_SIDEBAR_TITLE(obj)	       (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_SIDEBAR_TITLE))
#define NAUTILUS_IS_SIDEBAR_TITLE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SIDEBAR_TITLE))

typedef struct NautilusSidebarTitleDetails NautilusSidebarTitleDetails;

typedef struct
{
	GtkVBox box;
	NautilusSidebarTitleDetails *details; 
} NautilusSidebarTitle;

typedef struct
{
	GtkVBoxClass parent_class;
} NautilusSidebarTitleClass;

GType      nautilus_sidebar_title_get_type          (void);
GtkWidget *nautilus_sidebar_title_new               (void);
void       nautilus_sidebar_title_set_file          (NautilusSidebarTitle *sidebar_title,
						     NautilusFile         *file,
						     const char           *initial_text);
void       nautilus_sidebar_title_set_text          (NautilusSidebarTitle *sidebar_title,
						     const char           *new_title);
char *     nautilus_sidebar_title_get_text          (NautilusSidebarTitle *sidebar_title);
gboolean   nautilus_sidebar_title_hit_test_icon     (NautilusSidebarTitle *sidebar_title,
						     int                   x,
						     int                   y);
void       nautilus_sidebar_title_select_text_color (NautilusSidebarTitle *sidebar_title,
						     EelBackground        *background,
						     gboolean              is_default);

#endif /* NAUTILUS_SIDEBAR_TITLE_H */
