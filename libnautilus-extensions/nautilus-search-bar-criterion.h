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

#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkentry.h>


typedef enum {
	NAUTILUS_FILE_NAME_SEARCH_CRITERION,
	NAUTILUS_CONTENT_SEARCH_CRITERION,
	NAUTILUS_FILE_TYPE_SEARCH_CRITERION,
	NAUTILUS_LOCATION_SEARCH_CRITERION,
	NAUTILUS_SIZE_SEARCH_CRITERION,
	NAUTILUS_NOTES_SEARCH_CRITERION,
	NAUTILUS_EMBLEM_SEARCH_CRITERION,
	NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION,
	NAUTILUS_OWNER_SEARCH_CRITERION,
	NAUTILUS_LAST_CRITERION
} NautilusSearchBarCriterionType;

typedef struct NautilusSearchBarCriterionDetails NautilusSearchBarCriterionDetails;

#define NAUTILUS_SEARCH_BAR_CRITERION(arg) (NautilusSearchBarCriterion *) arg

typedef GList NautilusSearchBarCriterionList;

typedef struct NautilusSearchBarCriterion {
	NautilusSearchBarCriterionDetails *details;
	NautilusSearchBarCriterionType type;
} NautilusSearchBarCriterion;



NautilusSearchBarCriterion *       nautilus_search_bar_criterion_first_new        (void);

NautilusSearchBarCriterionList *   nautilus_search_bar_criterion_next_new          (NautilusSearchBarCriterionList *criteria);

void                               nautilus_search_bar_criterion_show             (NautilusSearchBarCriterion *criterion);
void                               nautilus_search_bar_criterion_hide             (NautilusSearchBarCriterion *criterion);

NautilusSearchBarCriterion *       nautilus_search_bar_criterion_list_get_last    (NautilusSearchBarCriterionList *criteria);
void                               nautilus_search_bar_criterion_destroy          (NautilusSearchBarCriterion *criterion);




#endif NAUTILUS_SEARCH_BAR_CRITERION_H
