/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-search-bar-criterion.h - Types that will
   bring up the various search criteria 

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
   License along with this program; see the file COPYING.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Rebecca Schulman <rebecka@eazel.com>
*/


#ifndef NAUTILUS_SEARCH_BAR_CRITERION_H
#define NAUTILUS_SEARCH_BAR_CRITERION_H

#include "nautilus-complex-search-bar.h"
#include <gtk/gtkeventbox.h>
#include <libnautilus-private/nautilus-entry.h>


typedef enum {
	NAUTILUS_FILE_NAME_SEARCH_CRITERION,
	NAUTILUS_CONTENT_SEARCH_CRITERION,
	NAUTILUS_FILE_TYPE_SEARCH_CRITERION,
	NAUTILUS_SIZE_SEARCH_CRITERION,
	NAUTILUS_EMBLEM_SEARCH_CRITERION,
	NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION,
	NAUTILUS_OWNER_SEARCH_CRITERION,
	NAUTILUS_NUMBER_OF_SEARCH_CRITERIA
} NautilusSearchBarCriterionType;


#define NAUTILUS_TYPE_SEARCH_BAR_CRITERION \
	(nautilus_search_bar_criterion_get_type ())
#define NAUTILUS_SEARCH_BAR_CRITERION(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_SEARCH_BAR_CRITERION, NautilusSearchBarCriterion))
#define NAUTILUS_SEARCH_BAR_CRITERION_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SEARCH_BAR_CRITERION, NautilusSearchBarCriterionClass))
#define NAUTILUS_IS_SEARCH_BAR_CRITERION(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_SEARCH_BAR_CRITERION))
#define NAUTILUS_IS_SEARCH_BAR_CRITERION_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SEARCH_BAR_CRITERION))


typedef struct NautilusSearchBarCriterionDetails NautilusSearchBarCriterionDetails;

typedef struct NautilusSearchBarCriterion {
	GtkEventBox parent_slot;
	NautilusSearchBarCriterionDetails *details;
} NautilusSearchBarCriterion;


typedef struct {
	GtkEventBoxClass parent_slot;
} NautilusSearchBarCriterionClass;


typedef void (* NautilusSearchBarCriterionCallback) (NautilusSearchBarCriterion *old_criterion,
						     gpointer data);

GtkType                            nautilus_search_bar_criterion_get_type         (void);

/* Three new procedures, each with a separate purpose:
   create the initial search option with first_new,
   create a new subsequent one automatically with next_new,
   and change to a particular type at a user's request with new_with_type */
NautilusSearchBarCriterion *       nautilus_search_bar_criterion_first_new        (NautilusComplexSearchBar *bar);

NautilusSearchBarCriterion *       nautilus_search_bar_criterion_next_new         (NautilusSearchBarCriterionType criterion_type,
										   NautilusComplexSearchBar *bar);
NautilusSearchBarCriterion *       nautilus_search_bar_criterion_new_with_type    (NautilusSearchBarCriterionType criteiron_type,
										   NautilusComplexSearchBar *bar);


char *                             nautilus_search_bar_criterion_get_location     (NautilusSearchBarCriterion *criterion);

void                               nautilus_search_bar_criterion_show             (NautilusSearchBarCriterion *criterion);
void                               nautilus_search_bar_criterion_hide             (NautilusSearchBarCriterion *criterion);

/* Run when a criteria changes so that we can be sure we give only valid criteria choices to the user,
   (like whether you can select name, type, etc. */
void                               nautilus_search_bar_criterion_update_valid_criteria_choices  (NautilusSearchBarCriterion *criterion,
												 GSList *current_criteria);


/* Search URI utilities. Maybe these should go in a separate file? */
char *                             nautilus_search_bar_criterion_human_from_uri   (const char *location_uri);

char *				   nautilus_search_uri_get_first_criterion 	  (const char *search_uri);

#endif /* NAUTILUS_SEARCH_BAR_CRITERION_H */
