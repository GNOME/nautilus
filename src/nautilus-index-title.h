/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus
 * Copyright (C) 2000 Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
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
 *
 * This is the header file for the index title panel, which is part of the index panel
 *
 */

#ifndef NAUTILUS_INDEX_TITLE_H
#define NAUTILUS_INDEX_TITLE_H


#include <gtk/gtkvbox.h>


#define NAUTILUS_TYPE_INDEX_TITLE	     (nautilus_index_title_get_type ())
#define NAUTILUS_INDEX_TITLE(obj)	     (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_INDEX_TITLE, NautilusIndexTitle))
#define NAUTILUS_INDEX_TITLE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_INDEX_TITLE, NautilusIndexTitleClass))
#define NAUTILUS_IS_INDEX_TITLE(obj)	     (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_INDEX_TITLE))
#define NAUTILUS_IS_INDEX_TITLE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_INDEX_TITLE))

typedef struct _NautilusIndexTitle NautilusIndexTitle;
typedef struct _NautilusIndexTitleClass NautilusIndexTitleClass;
typedef struct _NautilusIndexTitleDetails NautilusIndexTitleDetails;

struct _NautilusIndexTitle
{
	GtkVBox box;
	NautilusIndexTitleDetails *details; 
};

struct _NautilusIndexTitleClass
{
	GtkVBoxClass parent_class;
};


GtkType    nautilus_index_title_get_type (void);
GtkWidget* nautilus_index_title_new      (void);
void       nautilus_index_title_set_uri  (NautilusIndexTitle *index_title,
					  const char         *new_uri,
					  const char	     *initial_text);
void	   nautilus_index_title_set_text (NautilusIndexTitle *index_title,
					  const char	     *new_title);


#endif /* NAUTILUS_INDEX_TITLE_H */
