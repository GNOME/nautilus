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
#include <gtk/gtkeventbox.h>
#include "nautilus-complex-search-bar.h"
#include "nautilus-search-bar.h"


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

typedef enum {
	NAUTILUS_FILE_TYPE_SEARCH_CRITERION,
	NAUTILUS_LOCATION_SEARCH_CRITERION,
	NAUTILUS_CONTENT_SEARCH_CRITERION,
	NAUTILUS_SIZE_SEARCH_CRITERION,
	NAUTILUS_FILE_NAME_SEARCH_CRITERION,
	NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION,
	NAUTILUS_NOTES_SEARCH_CRITERION
} NautilusSearchBarCriterionType;

typedef struct NautilusSearchBarCriterionDisplay NautilusSearchBarCriterionDisplay;

/* FIXME:  Should I wrap casting functions for this type ? */
typedef GList NautilusSearchBarCriterionList;

typedef struct NautilusSearchBarCriterion {
	/* FIXME:  This is wrong */
	NautilusSearchBarCriterionDisplay *search_criterion_bar;
	NautilusSearchBarCriterionType type;
} NautilusSearchBarCriterion;
/*
typedef struct NautilusSearchBarCriterionClass {
	NautilusComplexSearchBar parent_class;
} NautilusSearchBarCriterionClass;
*/	

NautilusSearchBarCriterion *   nautilus_search_bar_criterion_file_type_new    (NautilusComplexSearchBar *bar);
NautilusSearchBarCriterion *   nautilus_search_bar_criterion_file_name_new    (NautilusComplexSearchBar *bar);

void                           nautilus_search_bar_criterion_add_to_container (GtkContainer *container, 
									       NautilusSearchBarCriterion *criterion);

#endif NAUTILUS_SEARCH_BAR_CRITERION_H

/*
Content: [contains, does not contain, [Text Field].

File Name: contains, does not contain, starts with, ends with, is, is not, matches pattern]


Size: [is less than, is greater than] [Text Field].

Item: [is, is not] [My files, My folders <divider> popup of common document
types, a divider, then MIME types].

Date Modified: [is, is not, is before, is after, is today, is yesterday]
[GtkCalendar].

Notes: [contain, do not contain] [Text Field].
*/


/*
union NautilusSearchCriterionDisplay {
	struct NautilusFileTypeCriterionDisplay file_display;
	struct NautilusLocationCriterionDisplay location_display;
	struct NautilusContentCriterionDisplay content_display;
	struct NautilusSizeCriterionDisplay size_display;
	struct NautilusFileNameCriterionDisplay file_name_display;
	struct NautilusDateModifiedDisplay date_modified_display;
	struct NautilusNotesDisplay notes_display;
};


struct NautilusFileTypeCriterionDisplay {
  GtkOptionMenu *option_menu;
};

struct NautilusLocationCriterionDisplay {
  GtkOptionMenu
};

struct NautilusSizeCriterionDisplay {

};

struct NautilusContentCriterionDisplay {

};

struct NautilusSizeCriterionDisplay {

};

struct NautilusFileNameCriterionDisplay {

};

struct NautilusDateModifiedDisplay {

};


*/

