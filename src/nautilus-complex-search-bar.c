/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-complex-search-bar.c - Search bar containing many attributes

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



#include "nautilus-complex-search-bar.h"

#include <glib.h>

#include <gtk/gtkeventbox.h>

#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-search-uri.h>
#include <libnautilus-extensions/nautilus-search-bar-criterion.h>
#include <libnautilus-extensions/nautilus-search-bar-criterion-private.h>

struct NautilusComplexSearchBarDetails {
	GtkVBox *bar_container;
	GtkTable *table;

	GtkWidget *more_options;
	GtkWidget *fewer_options;

	GSList *search_criteria;
	gchar *undo_text;
	gboolean undo_registered;
};

static char *                     nautilus_complex_search_bar_get_location            (NautilusComplexSearchBar *bar);
static void                       nautilus_complex_search_bar_set_search_controls     (NautilusSearchBar *bar,
										       const char *location);

static void                       nautilus_complex_search_bar_initialize_class        (NautilusComplexSearchBarClass *class);
static void                       nautilus_complex_search_bar_initialize              (NautilusComplexSearchBar      *bar);
static void                       attach_criterion_to_search_bar                      (NautilusComplexSearchBar *bar,
										       NautilusSearchBarCriterion *criterion,
										       int row_number);
static void                       unattach_criterion_from_search_bar                      (NautilusComplexSearchBar *bar,
											   NautilusSearchBarCriterion *criterion);

static void                       destroy                                             (GtkObject *object);
static void                       more_options_callback                               (GtkObject *object,
										       gpointer data);
static void                       fewer_options_callback                              (GtkObject *object,
										       gpointer data);


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
	
	klass->get_location = nautilus_complex_search_bar_get_location;
}

static void
destroy (GtkObject *object)
{
  
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}


static void
nautilus_complex_search_bar_initialize (NautilusComplexSearchBar *bar)
{
	NautilusSearchBarCriterion *file_name_criterion;
	GtkWidget *hbox;

	
	bar->details = g_new0 (NautilusComplexSearchBarDetails, 1);
	
	bar->details->bar_container = GTK_VBOX (gtk_vbox_new (FALSE, 0));

	bar->details->table = GTK_TABLE (gtk_table_new (1, 1, FALSE));
	file_name_criterion = nautilus_search_bar_criterion_first_new ();
	bar->details->search_criteria = g_slist_append (NULL,
							file_name_criterion);
	attach_criterion_to_search_bar (bar, file_name_criterion, 1);
	nautilus_search_bar_criterion_show (file_name_criterion);

	gtk_box_pack_start (GTK_BOX (bar->details->bar_container),
			    GTK_WIDGET (bar->details->table),
			    TRUE,
			    FALSE,
			    0);
	gtk_widget_show (GTK_WIDGET (bar->details->table));

	hbox = gtk_hbox_new (0, FALSE);

	bar->details->more_options = gtk_button_new_with_label ("More Options");
	gtk_signal_connect (GTK_OBJECT (bar->details->more_options), "clicked",
			    more_options_callback, bar);
	gtk_box_pack_start (GTK_BOX (hbox),
			     bar->details->more_options,
			     TRUE, FALSE, 0);
	gtk_widget_show (bar->details->more_options);
	
	bar->details->fewer_options = gtk_button_new_with_label ("Fewer Options");
	gtk_signal_connect (GTK_OBJECT (bar->details->fewer_options), "clicked",
			    fewer_options_callback, bar);
	gtk_box_pack_start (GTK_BOX (hbox),
			    bar->details->fewer_options,
			    TRUE, FALSE, 0);
	gtk_widget_show (bar->details->fewer_options);

	gtk_box_pack_start (GTK_BOX (bar->details->bar_container),
			    hbox,
			    TRUE,
			    FALSE,
			    0);
	gtk_widget_show (hbox);
	gtk_widget_show (GTK_WIDGET (bar->details->bar_container));
	gtk_container_add (GTK_CONTAINER (bar), GTK_WIDGET (bar->details->bar_container));
}


static char *                     
nautilus_complex_search_bar_get_location  (NautilusComplexSearchBar *bar)
{
	char *location;
	GSList *criteria;

	g_return_val_if_fail (bar->details->search_criteria != NULL, g_strdup ("search:[file:///]"));

	location = g_strdup ("search:[file:///]");
	for (criteria = bar->details->search_criteria; criteria != NULL; criteria = criteria->next) {
		/* FIXME: 1: It's missing a NULL on the end so it
		 * crashes unless you are "lucky". 2: It leaks because
		 * the old location isn't freed. Perhaps use GString
		 * instead? 3: It's 1/2-written code that doesn't do
		 * anything.
		 */
		/* location = g_strconcat (location, ""); */
	}

	/* FIXME: Not implemented yet. */
	return location;
}

static void                       
nautilus_complex_search_bar_set_search_controls (NautilusSearchBar *bar,
						const char *location)
{
	/* FIXME */
}

static void
attach_criterion_to_search_bar (NautilusComplexSearchBar *bar,
				NautilusSearchBarCriterion *criterion,
				int row)
{
	g_return_if_fail (NAUTILUS_IS_COMPLEX_SEARCH_BAR (bar));

	gtk_table_attach_defaults (bar->details->table,
				   GTK_WIDGET (criterion->details->available_option_menu),
				   0, 1, row - 1, row);
	
	gtk_table_attach_defaults (bar->details->table,
				   GTK_WIDGET (criterion->details->operator_menu),
				   1, 2, row - 1, row);
	g_assert (criterion->details->use_value_entry + 
		  criterion->details->use_value_menu == 1);
	if (criterion->details->use_value_entry) {
		gtk_table_attach_defaults (bar->details->table,
					   GTK_WIDGET (criterion->details->value_entry),
					   2, 3, row - 1, row);
	}
	if (criterion->details->use_value_menu) {
		gtk_table_attach_defaults (bar->details->table,
					   GTK_WIDGET (criterion->details->value_menu),
					   2, 3, row - 1, row);
	}
}

static void
unattach_criterion_from_search_bar (NautilusComplexSearchBar *bar,
				    NautilusSearchBarCriterion *criterion)
{
	gtk_container_remove (GTK_CONTAINER (bar->details->table),
			      GTK_WIDGET (criterion->details->available_option_menu));
	gtk_container_remove (GTK_CONTAINER (bar->details->table),
			      GTK_WIDGET (criterion->details->operator_menu));
	g_assert (criterion->details->use_value_entry + 
		  criterion->details->use_value_menu == 1);
	if (criterion->details->use_value_entry) {
		gtk_container_remove (GTK_CONTAINER (bar->details->table),
				      GTK_WIDGET (criterion->details->value_entry));
	}
	if (criterion->details->use_value_menu) {
		gtk_container_remove (GTK_CONTAINER (bar->details->table),
				      GTK_WIDGET (criterion->details->value_menu));
	}
}
				  

GtkWidget *
nautilus_complex_search_bar_new (void)
{
	return gtk_widget_new (NAUTILUS_TYPE_COMPLEX_SEARCH_BAR, NULL);
}


static void                       
more_options_callback (GtkObject *object,
		       gpointer data)
{
	NautilusSearchBarCriterion *criterion, *last_criterion;
	NautilusComplexSearchBar *bar;
	GSList *list;
  
	g_return_if_fail (GTK_IS_BUTTON (object));
	g_return_if_fail (NAUTILUS_IS_COMPLEX_SEARCH_BAR (data));

	bar = NAUTILUS_COMPLEX_SEARCH_BAR (data);

	list = bar->details->search_criteria;
	last_criterion = (NautilusSearchBarCriterion *)((g_slist_last (list))->data);
	criterion = nautilus_search_bar_criterion_next_new (last_criterion);
	bar->details->search_criteria = g_slist_append (list, criterion);

	attach_criterion_to_search_bar (bar, criterion, 
					g_slist_length (bar->details->search_criteria));
	nautilus_search_bar_criterion_show (criterion);

}


static void                       
fewer_options_callback (GtkObject *object,
		       gpointer data)
{
	NautilusSearchBarCriterion *criterion;
	NautilusComplexSearchBar *bar;
	GSList *last;
	int old_length, new_length;

	g_return_if_fail (GTK_IS_BUTTON (object));
	g_return_if_fail (NAUTILUS_IS_COMPLEX_SEARCH_BAR (data));

	bar = NAUTILUS_COMPLEX_SEARCH_BAR (data);

	old_length = g_slist_length (bar->details->search_criteria);
	if (old_length == 1) {
		return;
	}

	last = g_slist_last (bar->details->search_criteria);
	criterion = (NautilusSearchBarCriterion *) last->data;
	unattach_criterion_from_search_bar (bar, criterion);
	bar->details->search_criteria = g_slist_remove (bar->details->search_criteria, 
							criterion);

	/* Assert that the old criteria got removed from the criteria list */
	new_length = g_slist_length (bar->details->search_criteria);
	g_assert ( new_length + 1 == old_length);
	/* nautilus_search_bar_criterion_destroy (criterion); */
	gtk_table_resize (bar->details->table, 3, new_length);

	/* FIXME: the folowing is pretty much evil since it relies on the run-time
	   widget hierarchy of the nautilus toolbar. 
	   Any better fix is wellcome.
	*/
	gtk_widget_queue_resize (GTK_WIDGET (bar->details->bar_container)->parent->parent->parent->parent->parent->parent->parent->parent);
}





