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
	GtkWidget *hbox;
	NautilusSearchBarCriterionList *search_criteria;
	GtkButton *more_options;
	GtkOptionMenu *more_search_criteria_options;
	
	gchar *undo_text;
	gboolean undo_registered;
};

static char *                     nautilus_complex_search_bar_get_location            (NautilusComplexSearchBar *bar);
static char *                     nautilus_complex_search_bar_get_location            (NautilusComplexSearchBar *bar);


static void                       nautilus_complex_search_bar_initialize_class        (NautilusComplexSearchBarClass *class);
static void                       nautilus_complex_search_bar_initialize              (NautilusComplexSearchBar      *bar);
static void                       destroy                                             (GtkObject *object);
static void                       more_options_callback                               (GtkObject *object,
										       gpointer data);
static void                       add_file_type_search_criterion_callback             (GtkObject *object,
										       gpointer data);
static void                       add_file_name_search_criterion_callback             (GtkObject *object,
										       gpointer data);
static void                       add_file_location_search_criterion_callback         (GtkObject *object,
										       gpointer data);
static void                       add_content_search_criterion_callback               (GtkObject *object,
										       gpointer data);
static void                       add_size_search_criterion_callback                  (GtkObject *object,
										       gpointer data);
static void                       add_date_modified_search_criterion_callback         (GtkObject *object,
										       gpointer data);
static void                       add_notes_search_criterion_callback                 (GtkObject *object,
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
	NautilusSearchBarCriterion *file_type, *file_name;
	GtkWidget *more_options, *more_options_label;
	GtkWidget *hbox;

	hbox = gtk_hbox_new (0, FALSE);
	
	bar->details = g_new0 (NautilusComplexSearchBarDetails, 1);
	bar->details->search_criteria = NULL;

	file_type = nautilus_search_bar_criterion_file_type_new (bar);
	nautilus_search_bar_criterion_add_to_search_bar (file_type,
							 hbox);
	nautilus_search_bar_criterion_show (file_type);
	bar->details->search_criteria = g_list_append (bar->details->search_criteria,
						       file_type);


	file_name = nautilus_search_bar_criterion_file_name_new (bar);
	nautilus_search_bar_criterion_add_to_search_bar (file_name,
							 hbox);
	bar->details->search_criteria = g_list_append (bar->details->search_criteria,
						       file_name);
	nautilus_search_bar_criterion_show (file_name);

	
	more_options = gtk_button_new ();
	more_options_label = gtk_label_new ("More Options");
	gtk_signal_connect (GTK_OBJECT (more_options), "pressed",
			    more_options_callback, bar);
	gtk_container_add (GTK_CONTAINER (more_options), more_options_label);
	bar->details->more_options = GTK_BUTTON (more_options);
	gtk_box_pack_start (GTK_BOX (hbox), more_options, FALSE, TRUE, 1);
	bar->details->hbox = hbox;
	gtk_container_add (GTK_CONTAINER (bar), hbox); 

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
	NautilusComplexSearchBar *bar;
	GtkWidget *more_options_button;
	GtkWidget *more_search_criteria_options, *more_search_criteria_values;
	GtkWidget *file_type_item;
	GtkWidget *named_item, *located_in_item, *containing_item;
	GtkWidget *of_size_item, *modified_since_item, *notes_item;

	
	g_return_if_fail (GTK_IS_BUTTON (object));
	g_return_if_fail (NAUTILUS_IS_COMPLEX_SEARCH_BAR (data));

	bar = NAUTILUS_COMPLEX_SEARCH_BAR (data);
	more_options_button = GTK_WIDGET (object);

	gtk_widget_hide_all (more_options_button);

	if (bar->details->more_search_criteria_options == NULL) {
		more_search_criteria_options = gtk_option_menu_new ();
		
		more_search_criteria_values = gtk_menu_new ();
		
		file_type_item = gtk_menu_item_new_with_label ("File type");
		gtk_menu_append (GTK_MENU (more_search_criteria_values),
				 file_type_item);
				gtk_signal_connect (GTK_OBJECT (file_type_item),
			    "activate",
				    add_file_type_search_criterion_callback,
				    bar);
		
		named_item = gtk_menu_item_new_with_label ("Named");
		gtk_menu_append (GTK_MENU (more_search_criteria_values),
				 named_item);
		
		gtk_signal_connect (GTK_OBJECT (named_item),
				    "activate",
				    add_file_name_search_criterion_callback,
				    bar);
		
		located_in_item = gtk_menu_item_new_with_label ("Located in");
		gtk_menu_append (GTK_MENU (more_search_criteria_values),
				 located_in_item);
		
		gtk_signal_connect (GTK_OBJECT (located_in_item),
				    "activate",
				    add_file_location_search_criterion_callback,
			    bar); 
		
		containing_item = gtk_menu_item_new_with_label ("Containing");
		gtk_menu_append (GTK_MENU (more_search_criteria_values),
				 containing_item);
		
		gtk_signal_connect (GTK_OBJECT (containing_item),
				    "activate",
				    add_content_search_criterion_callback,
				    bar);
		
		of_size_item = gtk_menu_item_new_with_label ("Of Size");
		gtk_menu_append (GTK_MENU (more_search_criteria_values),
				 of_size_item);
		
		gtk_signal_connect (GTK_OBJECT (of_size_item),
				    "activate",
				    add_size_search_criterion_callback,
				    bar);
		
		modified_since_item = gtk_menu_item_new_with_label ("Modified Since");
		gtk_menu_append (GTK_MENU (more_search_criteria_values),
				 modified_since_item);
		
		gtk_signal_connect (GTK_OBJECT (modified_since_item),
				    "activate",
				    add_date_modified_search_criterion_callback,
				    bar);
		
		notes_item = gtk_menu_item_new_with_label ("With Note Containing");
		gtk_menu_append (GTK_MENU (more_search_criteria_values),
				 notes_item);
		
		gtk_signal_connect (GTK_OBJECT (notes_item),
				    "activate",
				    add_notes_search_criterion_callback,
				    bar);
		
		
		gtk_option_menu_set_menu (GTK_OPTION_MENU (more_search_criteria_options),	
					  more_search_criteria_values);
		/* We can use the enum indexes, since the above menu items are
		   in the same order as the enum struct.  */
		gtk_menu_set_active (GTK_MENU (more_search_criteria_values),
				     NAUTILUS_LOCATION_SEARCH_CRITERION);
		
		gtk_box_pack_start (GTK_BOX (bar->details->hbox), 
				    more_search_criteria_options, 
				    TRUE, TRUE, 1);
		bar->details->more_search_criteria_options = GTK_OPTION_MENU (more_search_criteria_options);	
	}

	gtk_widget_show_all (GTK_WIDGET (bar->details->more_search_criteria_options));
}
	

static void                       
add_file_type_search_criterion_callback (GtkObject *object,
					 gpointer data)
{
	NautilusComplexSearchBar *bar;
	NautilusSearchBarCriterion *file_type;

	
	printf ("Entering filetype search callback\n");
	g_return_if_fail (GTK_IS_MENU_ITEM (object));
	g_return_if_fail (NAUTILUS_IS_COMPLEX_SEARCH_BAR (data));
	
	bar = NAUTILUS_COMPLEX_SEARCH_BAR (data);

	gtk_widget_hide_all (GTK_WIDGET (bar->details->more_search_criteria_options));

	file_type = nautilus_search_bar_criterion_file_type_new (bar);
	nautilus_search_bar_criterion_add_to_search_bar (file_type,
							 bar->details->hbox);
	nautilus_search_bar_criterion_show (file_type);
	bar->details->search_criteria = g_list_append (bar->details->search_criteria,
						       file_type);
	gtk_widget_show_all (GTK_WIDGET (bar->details->more_options));
}


static void                       
add_file_name_search_criterion_callback (GtkObject *object,
					 gpointer data)
{
	printf ("Entering file name search callback\n");
}


static void                       
add_file_location_search_criterion_callback (GtkObject *object,
					 gpointer data)
{
	printf ("Entering location callback\n");
}

static void                       
add_content_search_criterion_callback (GtkObject *object,
				       gpointer data)
{
	printf ("Entering search criterion search callback\n");
}

static void                       
add_size_search_criterion_callback (GtkObject *object,
				    gpointer data)
{	printf ("Entering size criterion search callback\n");

}

static void                       
add_date_modified_search_criterion_callback (GtkObject *object,
					     gpointer data)
{
	printf ("Entering date modified search callback\n");
}

static void                       
add_notes_search_criterion_callback (GtkObject *object,
				   gpointer data)
{
	/* FIXME */
}


static char *                     
nautilus_complex_search_bar_get_location (NautilusComplexSearchBar *bar)
{
	return "file:///tmp";
}

