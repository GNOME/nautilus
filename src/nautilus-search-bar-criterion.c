/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-search-bar-criterion.c - Code to bring up
   the various kinds of criterion supported in the nautilus search
   bar 

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

#include "nautilus-search-bar-criterion.h"
#include "nautilus-search-bar.h"


struct NautilusSearchBarCriterionDisplay {
	GtkOptionMenu *criteria_available;
	GtkOptionMenu *operators_avaiable;
	gboolean value_is_in_entry;
	/* Only one of these are used at 
	   any given time */
	GtkOptionMenu *values_available;
	GtkEntry *entry_value;
};


NautilusSearchBarCriterion *
nautilus_search_bar_criterion_file_type_new (NautilusComplexSearchBar *bar)
{
	/* FIXME */
	return NULL;
  
}


NautilusSearchBarCriterion *
nautilus_search_bar_criterion_file_name_new (NautilusComplexSearchBar *bar)
{
	
	/* FIXME */
	return NULL;
  
}

void
nautilus_search_bar_criterion_add_to_container (GtkContainer *container, NautilusSearchBarCriterion *criterion)
{
	
}
