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


#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <gtk/gtkeventbox.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-search-bar-criterion.h>
#include <libnautilus-extensions/nautilus-search-bar-criterion-private.h>
#include <libnautilus-extensions/nautilus-string.h>

struct NautilusComplexSearchBarDetails {
	GtkVBox *bar_container;
	GtkTable *table;

	GtkWidget *more_options;
	GtkWidget *fewer_options;
	GtkWidget *find_them;

	GSList *search_criteria;
	gchar *undo_text;
	gboolean undo_registered;
};

static char *nautilus_complex_search_bar_get_location     (NautilusNavigationBar         *bar);
static void  nautilus_complex_search_bar_set_location     (NautilusNavigationBar         *bar,
							   const char                    *location);
static void  nautilus_complex_search_bar_initialize_class (NautilusComplexSearchBarClass *class);
static void  nautilus_complex_search_bar_initialize       (NautilusComplexSearchBar      *bar);
static void  nautilus_complex_search_bar_destroy 	  (GtkObject 			 *object);
static void  attach_criterion_to_search_bar               (NautilusComplexSearchBar      *bar,
							   NautilusSearchBarCriterion    *criterion,
							   int                            row_number);
static void  unattach_criterion_from_search_bar           (NautilusComplexSearchBar      *bar,
							   NautilusSearchBarCriterion    *criterion);
static void  more_options_callback                        (GtkObject                     *object,
							   gpointer                       data);
static void  fewer_options_callback                       (GtkObject                     *object,
							   gpointer                       data);
static void  criterion_callback                           (NautilusSearchBarCriterion *old_criterion,
							   NautilusSearchBarCriterion *new_criterion,
							   gpointer data);
static GtkWidget * load_find_them_pixmap_widget           (void);

static void	   update_options_buttons_state 	  (NautilusComplexSearchBar *bar);
static void	   update_find_button_state 	  	  (NautilusComplexSearchBar *bar);
static void	   update_dynamic_buttons_state 	  (NautilusComplexSearchBar *bar);


NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusComplexSearchBar, nautilus_complex_search_bar, NAUTILUS_TYPE_SEARCH_BAR)


     /* called by the criterion when the user choosed 
	a new criterion type */
static void criterion_callback (NautilusSearchBarCriterion *old_criterion,
				NautilusSearchBarCriterion *new_criterion,
				gpointer data)
{ 
	NautilusComplexSearchBar *bar;
	int row;
	GSList *criterion_list;
	GSList *list;

	bar = (NautilusComplexSearchBar *) data;

	unattach_criterion_from_search_bar (bar, old_criterion);
	
	criterion_list = g_slist_find (bar->details->search_criteria, 
				       old_criterion);
	row = g_slist_position (bar->details->search_criteria,
				criterion_list);
	g_print ("row %d\n", row);

	list = g_slist_remove (bar->details->search_criteria,
			       old_criterion);
			
	list = g_slist_insert (list, new_criterion, row);

	nautilus_search_bar_criterion_set_callback (new_criterion, 
						    criterion_callback,
						    (gpointer) bar);

	attach_criterion_to_search_bar (bar, new_criterion, row + 1);

}


     
     
static void
nautilus_complex_search_bar_initialize_class (NautilusComplexSearchBarClass *klass)
{
	GTK_OBJECT_CLASS (klass)->destroy = nautilus_complex_search_bar_destroy;

	NAUTILUS_NAVIGATION_BAR_CLASS (klass)->get_location = nautilus_complex_search_bar_get_location;
	NAUTILUS_NAVIGATION_BAR_CLASS (klass)->set_location = nautilus_complex_search_bar_set_location;
}

static void
nautilus_complex_search_bar_initialize (NautilusComplexSearchBar *bar)
{
	NautilusSearchBarCriterion *file_name_criterion;
	GtkWidget *hbox;
	GtkWidget *find_them_box, *find_them_pixmap_widget, *find_them_label;
	
	bar->details = g_new0 (NautilusComplexSearchBarDetails, 1);
	
	bar->details->bar_container = GTK_VBOX (gtk_vbox_new (FALSE, GNOME_PAD_SMALL));

	bar->details->table = GTK_TABLE (gtk_table_new (1, 3, TRUE));
	gtk_table_set_col_spacings (bar->details->table,
				    1);

	gtk_container_set_resize_mode (GTK_CONTAINER (bar->details->table),
				       GTK_RESIZE_IMMEDIATE);
	file_name_criterion = nautilus_search_bar_criterion_first_new ();
	nautilus_search_bar_criterion_set_callback (file_name_criterion, 
						    criterion_callback,
						    (gpointer) bar);

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

	hbox = gtk_hbox_new (FALSE, 1);


	bar->details->find_them = gtk_button_new ();
	find_them_box = gtk_hbox_new (FALSE, 1);
	find_them_pixmap_widget = load_find_them_pixmap_widget ();
	if (find_them_pixmap_widget != NULL)
		gtk_box_pack_start (GTK_BOX (find_them_box),
				    find_them_pixmap_widget,
				    TRUE,
				    FALSE,
				    1);
	find_them_label = gtk_label_new ("Find Them!");
	gtk_box_pack_start (GTK_BOX (find_them_box),
			    find_them_label,
			    TRUE,
			    FALSE,
			    1);

	gtk_container_add (GTK_CONTAINER (bar->details->find_them), find_them_box);
	gtk_signal_connect_object (GTK_OBJECT (bar->details->find_them), "pressed",
				   nautilus_navigation_bar_location_changed,
				   GTK_OBJECT (bar));

	gtk_box_pack_end (GTK_BOX (hbox), 
			  bar->details->find_them, FALSE, FALSE, 0);
	gtk_widget_show (bar->details->find_them);


	
	bar->details->fewer_options = gtk_button_new_with_label ("Fewer Options");
	gtk_signal_connect (GTK_OBJECT (bar->details->fewer_options), "clicked",
			    fewer_options_callback, bar);

	gtk_box_pack_end (GTK_BOX (hbox),
			    bar->details->fewer_options,
			    FALSE, FALSE, 2 * GNOME_PAD_SMALL);

	gtk_widget_show (bar->details->fewer_options);

	bar->details->more_options = gtk_button_new_with_label ("More Options");
	gtk_signal_connect (GTK_OBJECT (bar->details->more_options), "clicked",
			    more_options_callback, bar);
				
				
	gtk_box_pack_end (GTK_BOX (hbox),
			     bar->details->more_options,
			     FALSE, FALSE, 0);
	gtk_widget_show (bar->details->more_options);

	gtk_box_pack_start (GTK_BOX (bar->details->bar_container),
			    hbox,
			    TRUE,
			    FALSE,
			    0);
	gtk_widget_show (hbox);
	gtk_widget_show (GTK_WIDGET (bar->details->bar_container));
	gtk_container_add (GTK_CONTAINER (bar), GTK_WIDGET (bar->details->bar_container));

	update_dynamic_buttons_state (bar);
}

static void
nautilus_complex_search_bar_destroy (GtkObject *object)
{
	g_free (NAUTILUS_COMPLEX_SEARCH_BAR (object)->details);
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* returned string should be g_freed by the caller */
static char *
nautilus_complex_search_bar_get_location (NautilusNavigationBar *navigation_bar)
{
	NautilusComplexSearchBar *bar;
	char *criteria_text, *trimmed_fragment, *escaped_fragment;
	char *search_uri;
	GSList *list;

	bar = NAUTILUS_COMPLEX_SEARCH_BAR (navigation_bar);

	criteria_text = g_strdup ("[file:///]");

	for (list = bar->details->search_criteria; list != NULL; list = list->next) {
		char *temp_criterion, *criterion_text;
		NautilusSearchBarCriterion *criterion;
		criterion = (NautilusSearchBarCriterion *) list->data;
		criterion_text = nautilus_search_bar_criterion_get_location (criterion);
		temp_criterion = g_strconcat (criteria_text, criterion_text, " & ", NULL);
		g_free (criteria_text);
		g_free (criterion_text);
		criteria_text = temp_criterion;
	}
	trimmed_fragment = nautilus_str_strip_trailing_str (criteria_text, " & ");
	g_free (criteria_text);

	escaped_fragment = gnome_vfs_escape_string (trimmed_fragment);
	search_uri = g_strconcat ("search:", escaped_fragment, NULL);
	g_free (escaped_fragment);
	return search_uri;
}

static void                       
nautilus_complex_search_bar_set_location (NautilusNavigationBar *navigation_bar,
					  const char *location)
{
	NautilusComplexSearchBar *bar;

	bar = NAUTILUS_COMPLEX_SEARCH_BAR (navigation_bar);

	/* FIXME: Not implemented. */
}

static void
attach_criterion_to_search_bar (NautilusComplexSearchBar *bar,
				NautilusSearchBarCriterion *criterion,
				int row)
{

	g_return_if_fail (NAUTILUS_IS_COMPLEX_SEARCH_BAR (bar));

	gtk_table_attach_defaults (bar->details->table,
				   GTK_WIDGET (criterion->details->available_criteria),
				   0, 1, row - 1, row);
	
	gtk_table_attach_defaults  (bar->details->table,
				    GTK_WIDGET (criterion->details->relation_menu),
				    1, 2, row - 1, row);
	g_assert (criterion->details->use_value_entry + 
		  criterion->details->use_value_menu == 1);
	if (criterion->details->use_value_entry) {
		gtk_table_attach_defaults (bar->details->table,
					   GTK_WIDGET (criterion->details->value_entry),
					   2, 3, row - 1, row);
		/* We want to track whether the entry text is empty or not. */
		gtk_signal_connect_object (GTK_OBJECT (criterion->details->value_entry),
				    	   "changed", 
				    	   update_find_button_state, 
				    	   GTK_OBJECT (bar));
	}
	if (criterion->details->use_value_menu) {
		gtk_table_attach_defaults (bar->details->table,
					   GTK_WIDGET (criterion->details->value_menu),
					   2, 3, row - 1, row);
	}
	gtk_table_resize (bar->details->table, row, 3);

}

static void
unattach_criterion_from_search_bar (NautilusComplexSearchBar *bar,
				    NautilusSearchBarCriterion *criterion)
{
	gtk_container_remove (GTK_CONTAINER (bar->details->table),
			      GTK_WIDGET (criterion->details->available_criteria));

	gtk_container_remove (GTK_CONTAINER (bar->details->table),
			      GTK_WIDGET (criterion->details->relation_menu));
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
	gtk_table_resize (bar->details->table, 
			  g_slist_length (bar->details->search_criteria) - 1, 
			  3);

}

static GtkWidget *
load_find_them_pixmap_widget (void)
{
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	
	pixbuf = gdk_pixbuf_new_from_file (NAUTILUS_PIXMAPDIR "/search.png");
	if(pixmap != NULL) {
		gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, 128);
		gdk_pixbuf_unref (pixbuf);
		return gtk_pixmap_new (pixmap, mask);
	} else
		return NULL;
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
	criterion = nautilus_search_bar_criterion_next_new (last_criterion->details->type);
	nautilus_search_bar_criterion_set_callback (criterion, 
						    criterion_callback, 
						    (gpointer) bar);
	bar->details->search_criteria = g_slist_append (list, criterion);

	attach_criterion_to_search_bar (bar, criterion, 
					g_slist_length (bar->details->search_criteria));
	nautilus_search_bar_criterion_show (criterion);
	update_dynamic_buttons_state (bar);
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
	g_assert (new_length + 1 == old_length);

	/* FIXME: the folowing is pretty much evil since it relies on the run-time
	   widget hierarchy of the nautilus toolbar. 
	   Any better fix is wellcome.
	*/
	gtk_widget_queue_resize (GTK_WIDGET (bar->details->bar_container)->parent->parent->parent->parent->parent->parent->parent->parent);

	update_dynamic_buttons_state (bar);
}

static void
update_options_buttons_state (NautilusComplexSearchBar *bar)
{
	/* "Fewer Options" is enabled unless there's only one criterion */
	gtk_widget_set_sensitive (GTK_WIDGET (bar->details->fewer_options), g_slist_length (bar->details->search_criteria) > 1);

	/* FIXME:
	 * When we limit the number of criteria available for a single search, we need to
	 * sensitize the "More Options" button correctly here.
	 */
}

static gboolean
criteria_invalid (NautilusComplexSearchBar *bar)
{
	GSList *node;
	NautilusSearchBarCriterion *criterion;
	char *text;
	gboolean text_is_empty;
	
	g_assert (NAUTILUS_IS_COMPLEX_SEARCH_BAR (bar));

	/* Walk through all value fields, checking whether any of them are empty.
	 * Maybe someday we will also check the non-text fields to make sure 
	 * they are set to sensible values somehow. 
	 */
	for (node = bar->details->search_criteria; node != NULL; node = node->next) {
		criterion = NAUTILUS_SEARCH_BAR_CRITERION (node->data);
		if (criterion->details->use_value_entry) {
			text = gtk_editable_get_chars 
				(GTK_EDITABLE (criterion->details->value_entry),
				0, -1);
			text_is_empty = nautilus_str_is_empty (text);
			g_free (text);
			if (text_is_empty) {
				return TRUE;
			}
		}
	}

	return FALSE;
}

static void
update_find_button_state (NautilusComplexSearchBar *bar)
{
	/* "Find" button is enabled only if the criteria are valid. */
	gtk_widget_set_sensitive (GTK_WIDGET (bar->details->find_them), !criteria_invalid (bar));
}

static void
update_dynamic_buttons_state (NautilusComplexSearchBar *bar)
{
	update_options_buttons_state (bar);
	update_find_button_state (bar);
}



