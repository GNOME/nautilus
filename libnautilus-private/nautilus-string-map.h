/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-string-map.h: A many-to-one string mapping data structure.
 
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
  
   Author: Ramiro Estrugo <ramiro@eazel.com>
*/

/* NautilusStringMap is a simple data structure to manage many-to-one 
 * mappings of string.  For example:
 * 
 * map = map_new (TRUE);
 * map_add (map, "animal", "dog");
 * map_add (map, "animal", "cat");
 * map_add (map, "animal", "mouse");
 * 
 * map_lookup (map, "dog") => "animal"
 * map_lookup (map, "cat") => "animal"
 * map_lookup (map, "animal") => "animal"
 *
 */

#ifndef NAUTILUS_STRING_MAP_H
#define NAUTILUS_STRING_MAP_H

#include <glib.h>

/* Opaque type declaration. */
typedef struct _NautilusStringMap NautilusStringMap;

/* Construct an empty string map. */
NautilusStringMap *nautilus_string_map_new    (gboolean                 case_sensitive);

/* Add a mapping from 'string' to 'strings_maps_to' */
void               nautilus_string_map_add    (NautilusStringMap       *string_map,
					       const char              *string_maps_to,
					       const char              *string);

/* Free the string map */
void               nautilus_string_map_free   (NautilusStringMap       *string_map);


/* Clear the string map */
void               nautilus_string_map_clear  (NautilusStringMap       *string_map);


/* Lookup the string in the map.  Returns the string 'string' maps to or NULL of not found. */
char *             nautilus_string_map_lookup (const NautilusStringMap *string_map,
					       const char              *string);


#endif /* NAUTILUS_STRING_MAP_H */

