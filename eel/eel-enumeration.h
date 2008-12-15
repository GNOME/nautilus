/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   eel-enumeration.h: Enumeration data structure.
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
  
   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef EEL_ENUMERATION_H
#define EEL_ENUMERATION_H

#include <glib.h>

/* Opaque EelEnumeration declaration. */
typedef struct EelEnumeration EelEnumeration;

typedef struct
{
	char *name;
	char *description;
	guint value;
} EelEnumerationEntry;

char *          eel_enumeration_get_id                            (const EelEnumeration      *enumeration);

guint           eel_enumeration_get_length                        (const EelEnumeration      *enumeration);
const EelEnumerationEntry *
                eel_enumeration_get_nth_entry                     (const EelEnumeration      *enumeration,
								   guint                      n);
int             eel_enumeration_get_name_position                 (const EelEnumeration      *enumeration,
								   const char                *name);
gboolean        eel_enumeration_contains_name                     (const EelEnumeration      *enumeration,
								   const char                *name);
guint           eel_enumeration_get_value_for_name                (const EelEnumeration      *enumeration,
								   const char                *name);
const char *    eel_enumeration_get_name_for_value                (const EelEnumeration      *enumeration,
								   int                        value);
char **         eel_enumeration_get_names                         (const EelEnumeration      *enumeration);

void            eel_enumeration_register                          (const char                *id,
								   const EelEnumerationEntry  entries[],
								   guint                      n_entries);
const EelEnumeration *
                eel_enumeration_lookup                            (const char                *id);

#endif /* EEL_ENUMERATION_H */

