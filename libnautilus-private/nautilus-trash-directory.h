/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-trash-directory.h: Subclass of NautilusDirectory to implement
   the virtual trash directory.
 
   Copyright (C) 1999, 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#ifndef NAUTILUS_TRASH_DIRECTORY_H
#define NAUTILUS_TRASH_DIRECTORY_H

#include "nautilus-merged-directory.h"

#define NAUTILUS_TYPE_TRASH_DIRECTORY \
	(nautilus_trash_directory_get_type ())
#define NAUTILUS_TRASH_DIRECTORY(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_TRASH_DIRECTORY, NautilusTrashDirectory))
#define NAUTILUS_TRASH_DIRECTORY_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_TRASH_DIRECTORY, NautilusTrashDirectoryClass))
#define NAUTILUS_IS_TRASH_DIRECTORY(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_TRASH_DIRECTORY))
#define NAUTILUS_IS_TRASH_DIRECTORY_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_TRASH_DIRECTORY))

typedef struct NautilusTrashDirectoryDetails NautilusTrashDirectoryDetails;

typedef struct {
	NautilusMergedDirectory parent_slot;
	NautilusTrashDirectoryDetails *details;
} NautilusTrashDirectory;

typedef struct {
	NautilusMergedDirectoryClass parent_slot;
} NautilusTrashDirectoryClass;

GtkType nautilus_trash_directory_get_type (void);
void	nautilus_trash_directory_finish_initializing (NautilusTrashDirectory *trash);

#endif /* NAUTILUS_TRASH_DIRECTORY_H */
