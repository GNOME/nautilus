/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-simple-search-bar.c - One box Search bar for Nautilus

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
#include "nautilus-complex-search-bar.h"

#include <glib.h>

#include <gtk/gtkeventbox.h>

#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-search-uri.h>


struct NautilusComplexSearchBarDetails {
  
  NautilusSearchBarCriterionList *search_criteria;
	
  gchar *undo_text;
  gboolean undo_registered;
};


static void                       nautilus_complex_search_bar_set_search_controls     (NautilusSearchBar *bar,
										       const char            *location);


static void                       nautilus_complex_search_bar_initialize_class        (NautilusComplexSearchBarClass *class);
static void                       nautilus_complex_search_bar_initialize              (NautilusComplexSearchBar      *bar);
static void                       destroy                                             (GtkObject *object);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusComplexSearchBar, nautilus_complex_search_bar, NAUTILUS_TYPE_SEARCH_BAR)



static void
nautilus_complex_search_bar_initialize_class (NautilusComplexSearchBarClass *klass)
{
	GtkObjectClass *object_class;
	NautilusSearchBarClass *search_bar_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	search_bar_class = NAUTILUS_SEARCH_BAR_CLASS (klass);
	search_bar_class->set_search_controls = nautilus_complex_search_bar_set_search_controls;
}

static void
destroy (GtkObject *object)
{
  
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}


static void
nautilus_complex_search_bar_initialize (NautilusComplexSearchBar *bar)
{
	NautilusSearchBarCriterionList *file_name_criterion;
	
	bar->details = g_new0 (NautilusComplexSearchBarDetails, 1);
	bar->details->search_criteria = NULL;
	bar->details->search_criteria = g_list_append (bar->details->search_criteria,
						       nautilus_search_bar_criterion_file_type_new (bar));
	/*	nautilus_search_bar_criterion_file_type_show (bar->details->search_criteria->data); */
	nautilus_search_bar_criterion_add_to_container (GTK_CONTAINER (bar),
							bar->details->search_criteria->data);

	bar->details->search_criteria = g_list_append (bar->details->search_criteria,
						       nautilus_search_bar_criterion_file_name_new (bar));	
	file_name_criterion = bar->details->search_criteria->next;
	/*	nautilus_search_bar_criterion_file_name_show (file_name_criterion->data); */
	nautilus_search_bar_criterion_add_to_container (GTK_CONTAINER (bar), file_name_criterion->data);
						     
}

GtkWidget *
nautilus_complex_search_bar_new (void)
{
	return gtk_widget_new (NAUTILUS_TYPE_COMPLEX_SEARCH_BAR, NULL);
}


static void                       
nautilus_complex_search_bar_set_search_controls  (NautilusSearchBar *bar,
						  const char *location)
{
	/* FIXME */
}
