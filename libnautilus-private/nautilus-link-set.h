/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-link-set.h: xml-based sets of link files
 
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

#ifndef NAUTILUS_LINK_SET_H
#define NAUTILUS_LINK_SET_H

#include <glib.h>
#include <gtk/gtkwindow.h>

gboolean	nautilus_link_set_install (const char *directory_path,
				    	const char *link_set_name);
void		nautilus_link_set_remove  (const char *directory_path,
				    	const char *link_set_name);
gboolean	nautilus_link_set_is_installed (const char *directory_path, 
					const char *link_set_name);
					
#endif /* NAUTILUS_LINK_SET_H */
