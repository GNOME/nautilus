/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-annotation.h: routines for getting and setting xml-based annotations associated
   with the digest of a file.
 
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
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Authors: Andy Hertzfeld <andy@eazel.com>
*/

#ifndef NAUTILUS_ANNOTATION_H
#define NAUTILUS_ANNOTATION_H

#include <glib.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-metadata.h>

char	*nautilus_annotation_get_annotation (NautilusFile *file);
int	nautilus_annotation_has_annotation (NautilusFile *file);
void	nautilus_annotation_add_annotation  (NautilusFile *file, const char *new_annotation);
void	nautilus_annotation_remove_annotation (NautilusFile *file, int which_annotation);

#endif /* NAUTILUS_ANNOTATION_H */
