
/*
 * SPDX-FileCopyrightText: 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 *         Ettore Perazzoli <ettore@gnu.org>
 */

#pragma once

#include <gtk/gtk.h>

#define NAUTILUS_TYPE_LOCATION_ENTRY nautilus_location_entry_get_type()
G_DECLARE_DERIVABLE_TYPE (NautilusLocationEntry, nautilus_location_entry,
                          NAUTILUS, LOCATION_ENTRY,
                          GtkEntry)

typedef struct _NautilusLocationEntryClass {
	GtkEntryClass parent_class;
	/* for GtkBindingSet */
	void         (* cancel)           (NautilusLocationEntry *entry);
} NautilusLocationEntryClass;

typedef enum {
	NAUTILUS_LOCATION_ENTRY_ACTION_GOTO,
	NAUTILUS_LOCATION_ENTRY_ACTION_CLEAR
} NautilusLocationEntryAction;

GtkWidget* nautilus_location_entry_new          	(void);
void       nautilus_location_entry_set_special_text     (NautilusLocationEntry *entry,
							 const char            *special_text);
void       nautilus_location_entry_set_secondary_action (NautilusLocationEntry *entry,
							 NautilusLocationEntryAction secondary_action);
void       nautilus_location_entry_set_location         (NautilusLocationEntry *entry,
							 GFile                 *location);
