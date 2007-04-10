/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 *         Ettore Perazzoli <ettore@gnu.org>
 */

#ifndef NAUTILUS_LOCATION_ENTRY_H
#define NAUTILUS_LOCATION_ENTRY_H

#include <libnautilus-private/nautilus-entry.h>

#define NAUTILUS_TYPE_LOCATION_ENTRY (nautilus_location_entry_get_type ())
#define NAUTILUS_LOCATION_ENTRY(obj) \
	GTK_CHECK_CAST (obj, NAUTILUS_TYPE_LOCATION_ENTRY, NautilusLocationEntry)
#define NAUTILUS_LOCATION_ENTRY_CLASS(klass) \
	GTK_CHECK_CLASS_CAST (klass, NAUTILUS_TYPE_LOCATION_ENTRY, NautilusLocationEntryClass)
#define NAUTILUS_IS_LOCATION_ENTRY(obj) \
	GTK_CHECK_TYPE (obj, NAUTILUS_TYPE_LOCATION_ENTRY)

typedef struct NautilusLocationEntryDetails NautilusLocationEntryDetails;

typedef struct NautilusLocationEntry {
	NautilusEntry parent;
	NautilusLocationEntryDetails *details;
} NautilusLocationEntry;

typedef struct {
	NautilusEntryClass parent_class;
} NautilusLocationEntryClass;

GType      nautilus_location_entry_get_type     	(void);
GtkWidget* nautilus_location_entry_new          	(void);
void       nautilus_location_entry_set_special_text     (NautilusLocationEntry *entry,
							 const char            *special_text);

#endif /* NAUTILUS_LOCATION_ENTRY_H */
