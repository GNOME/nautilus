/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-enumeration.h: Enumeration data structure.
 
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
  
   Author: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef NAUTILUS_ENUMERATION_H
#define NAUTILUS_ENUMERATION_H

#include <libnautilus-extensions/nautilus-string-list.h>

/* Opaque NautilusEnumeration declaration. */
typedef struct NautilusEnumeration NautilusEnumeration;

NautilusEnumeration *nautilus_enumeration_new                 (void);
void                 nautilus_enumeration_free                (NautilusEnumeration       *enumeration);
void                 nautilus_enumeration_insert              (NautilusEnumeration       *enumeration,
							       const char                *entry,
							       const char                *description,
							       int                        value);
char *               nautilus_enumeration_get_nth_entry       (const NautilusEnumeration *enumeration,
							       guint                      n);
char *               nautilus_enumeration_get_nth_description (const NautilusEnumeration *enumeration,
							       guint                      n);
int                  nautilus_enumeration_get_nth_value       (const NautilusEnumeration *enumeration,
							       guint                      n);
guint                nautilus_enumeration_get_num_entries     (const NautilusEnumeration *enumeration);
NautilusEnumeration *nautilus_enumeration_new_from_tokens     (const char                *entries,
							       const char                *descriptions,
							       const char                *values,
							       const char                *delimiter);
int                  nautilus_enumeration_get_entry_position  (const NautilusEnumeration *enumeration,
							       const char                *entry);
int                  nautilus_enumeration_get_value_position  (const NautilusEnumeration *enumeration,
							       int                        value);
NautilusStringList * nautilus_enumeration_get_entries         (const NautilusEnumeration *enumeration);

#endif /* NAUTILUS_ENUMERATION_H */

