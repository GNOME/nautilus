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

#include <gtk/gtkentry.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkmenu.h>

#include <libnautilus-extensions/nautilus-gtk-macros.h>



struct NautilusSearchBarCriterionDetails {
	gboolean use_intro_label;
	GtkLabel *intro_label;

	gboolean use_operator_menu;
	GtkOptionMenu *operator_menu;

	gboolean use_value_entry;
	GtkEntry *entry_value;
	gboolean use_value_menu;
	GtkOptionMenu *values_available;
};

static void      destroy                                        (GtkObject *object);
static void      nautilus_search_bar_criterion_initialize       (NautilusSearchBarCriterion *criterion);
static void      nautilus_search_bar_criterion_initialize_class (NautilusSearchBarCriterionClass *klass);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSearchBarCriterion, nautilus_search_bar_criterion, GTK_TYPE_CONTAINER)






static void
nautilus_search_bar_criterion_initialize (NautilusSearchBarCriterion *criterion)
{
	/* Nothing to do until a type of bar is declared */
}

static void
nautilus_search_bar_criterion_initialize_class (NautilusSearchBarCriterionClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;
}



static void
destroy (GtkObject *object)
{
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

GtkWidget *
nautilus_search_bar_criterion_new (void)
{
	return gtk_widget_new (NAUTILUS_TYPE_SEARCH_BAR_CRITERION, NULL);
}



NautilusSearchBarCriterion *
nautilus_search_bar_criterion_file_type_new (NautilusComplexSearchBar *bar)
{
     GtkWidget *new_criterion;
     NautilusSearchBarCriterion *file_type;
     GtkWidget *file_type_menu, *file_type_options;
     
     new_criterion = nautilus_search_bar_criterion_new ();
     
     file_type = NAUTILUS_SEARCH_BAR_CRITERION (new_criterion);
     
     file_type->details = g_new0 (NautilusSearchBarCriterionDetails, 1);

     file_type->type = NAUTILUS_FILE_TYPE_SEARCH_CRITERION;
     
     file_type_options = gtk_option_menu_new ();
	
     file_type_menu = gtk_menu_new ();
     gtk_menu_append (GTK_MENU (file_type_menu),
		      gtk_menu_item_new_with_label ("Files"));
     gtk_menu_append (GTK_MENU (file_type_menu),
			 gtk_menu_item_new_with_label ("Text Files"));
     gtk_menu_append (GTK_MENU (file_type_menu),
		      gtk_menu_item_new_with_label ("Applications"));
     gtk_menu_append (GTK_MENU (file_type_menu),
		      gtk_menu_item_new_with_label ("Music"));
     gtk_menu_append (GTK_MENU (file_type_menu),
		      gtk_menu_item_new_with_label ("Directories"));
     gtk_option_menu_set_menu (GTK_OPTION_MENU (file_type_options),
			       file_type_menu);

     file_type->details->operator_menu = GTK_OPTION_MENU (file_type_options);
     file_type->details->use_operator_menu = TRUE;

     file_type->details->use_intro_label = FALSE;
     file_type->details->use_value_entry = FALSE;
     file_type->details->use_value_menu = FALSE;
     return file_type;
}

NautilusSearchBarCriterion *
nautilus_search_bar_criterion_file_name_new (NautilusComplexSearchBar *bar)
{
	GtkWidget *new_criterion;
	GtkWidget *intro_label;
	NautilusSearchBarCriterion *file_name;
	GtkWidget *file_name_menu, *file_name_options, *file_name_entry;

	new_criterion = nautilus_search_bar_criterion_new ();
	
	file_name = NAUTILUS_SEARCH_BAR_CRITERION (new_criterion);

	file_name->details = g_new0 (NautilusSearchBarCriterionDetails, 1);

	file_name->type = NAUTILUS_FILE_NAME_SEARCH_CRITERION;

	intro_label = gtk_label_new ("where name");
	file_name->details->intro_label = GTK_LABEL (intro_label);
	file_name->details->use_intro_label = TRUE;

	file_name_options = gtk_option_menu_new ();

	file_name_menu = gtk_menu_new ();
	gtk_menu_append (GTK_MENU (file_name_menu),
			 gtk_menu_item_new_with_label ("contains"));
	gtk_menu_append (GTK_MENU (file_name_menu),
			 gtk_menu_item_new_with_label ("does not contain"));
	gtk_menu_append (GTK_MENU (file_name_menu),
			 gtk_menu_item_new_with_label ("is"));
	gtk_menu_append (GTK_MENU (file_name_menu),
			 gtk_menu_item_new_with_label ("is not"));
	gtk_menu_append (GTK_MENU (file_name_menu),
			 gtk_menu_item_new_with_label ("matches pattern"));

	gtk_option_menu_set_menu (GTK_OPTION_MENU (file_name_options),
				  file_name_menu);
	file_name->details->operator_menu = GTK_OPTION_MENU (file_name_options);
	file_name->details->use_operator_menu = TRUE;

	file_name_entry = gtk_entry_new ();
	file_name->details->entry_value = GTK_ENTRY (file_name_entry);
	file_name->details->use_value_entry = TRUE;

	file_name->details->use_value_menu = FALSE;

	return file_name;
}

void
nautilus_search_bar_criterion_add_to_search_bar (NautilusSearchBarCriterion *criterion,
						 GtkWidget *hbox)
{
	g_return_if_fail (NAUTILUS_IS_SEARCH_BAR_CRITERION (criterion));
	g_return_if_fail (GTK_IS_BOX (hbox));
	if (criterion->details->use_intro_label) {
		gtk_box_pack_start (GTK_BOX (hbox),
				    GTK_WIDGET (criterion->details->intro_label),
				    TRUE,
				    FALSE,
				    GNOME_PAD_SMALL);
	}
	if (criterion->details->use_operator_menu) {
		gtk_box_pack_start (GTK_BOX (hbox),
				    GTK_WIDGET (criterion->details->operator_menu),
				    TRUE,
				    FALSE,
				    GNOME_PAD_SMALL);
	}
	if (criterion->details->use_value_entry) {
		gtk_box_pack_start (GTK_BOX (hbox),
				     GTK_WIDGET (criterion->details->entry_value),
				     TRUE,
				     FALSE,
				     GNOME_PAD_SMALL);
	}
	if (criterion->details->use_value_menu) {
		gtk_box_pack_start (GTK_BOX (hbox),
				     GTK_WIDGET (criterion->details->values_available),
				     TRUE,
				     TRUE,
				     0);
	}
	
}


void
nautilus_search_bar_criterion_add_to_container (GtkContainer *container, NautilusSearchBarCriterion *criterion)
{
	
}
