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

#include "nautilus-gtk-macros.h"
#include "nautilus-search-bar-criterion.h"
#include "nautilus-search-bar-criterion-private.h"

#include <gtk/gtksignal.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

#include <libgnomeui/gnome-uidefs.h>

static char * criteria_titles [] = {
	  N_("Name"),
	  N_("Content"),
	  N_("Type"),
	  N_("Stored"),
	  N_("Size"),
	  N_("With Emblem"),
	  N_("Last Modified"),
	  N_("Owned By"),
	  NULL
};


static char *name_relations [] = {
	N_("contains"),
	N_("starts with"),
	N_("ends with"),
	N_("matches glob"),
	N_("matches regexp"),
	NULL
};

static char *content_relations [] = {
	N_("includes"),
	N_("does not include"),
	NULL

};

static char *type_relations [] = {
	N_("is"),
	N_("is not"),
	NULL
};

static char *type_objects [] = {
	N_("regular file"),
	N_("text file"),
	N_("application"),
	N_("directory"),
	N_("music"),
	NULL
};

static char *location_relations [] = {
	N_("is"),
	NULL
};

static char *location_objects [] = {
	N_("on this computer"),
	N_("in my vault"),
	NULL
};

static char *size_relations [] = {
	N_("larger than"),
	N_("smaller than"),
	NULL
};

static char *size_objects [] = {
	N_("1 KB"),
	N_("10 KB"),
	N_("100 KB"),
	N_("1 MB"),
	N_("10 MB"),
	N_("100 MB"),
	NULL
};



static char *emblem_relations [] = {
	N_("marked with"),
	N_("not marked with"),
	NULL
};

static char *emblem_objects [] = {
	/* FIXME: add emblem possibilities here.
	   likely to be icon filenames
	*/
	NULL
};
	
static char *modified_relations [] = {
	N_("after"),
	N_("before"),
	NULL
};

static char *modified_objects [] = {
	N_("today"),
	N_("this week"),
	N_("this month"),
	NULL
};

static char *owner_relations [] = {
	N_("is"),
	N_("is not"),
	NULL
};

static NautilusSearchBarCriterion * nautilus_search_bar_criterion_new              (void);

static NautilusSearchBarCriterion * nautilus_search_bar_criterion_new_from_values  (NautilusSearchBarCriterionType type,
										    char *operator_options[],
										    gboolean use_value_entry,
										    gboolean use_value_menu,
										    char *value_options[]);

NautilusSearchBarCriterionType      get_next_default_search_criterion_type         (NautilusSearchBarCriterionType type) ;

static char *                              get_name_location_for                  (int relation_number,
										   char *name_text);
static char *                              get_content_location_for               (int relation_number,
										   char *name_text);
static char *                              get_file_type_location_for             (int relation_number,
										   int value_number);
static char *                              get_size_location_for                  (int relation_number,
										   char *size_text);
static char *                              get_emblem_location_for                (int relation_number,
										   int value_number);
static char *                              get_date_modified_location_for         (int relation_number,
										   int value_number);
static char *                              get_owner_location_for                 (int relation_number,
										   char *owner_number);



void
nautilus_search_bar_criterion_destroy (NautilusSearchBarCriterion *criterion)
{
	/* FIXME : need more freeage */
	g_free (criterion->details);
	g_free (criterion);
}

static NautilusSearchBarCriterion * 
nautilus_search_bar_criterion_new (void)
{
	return g_new0 (NautilusSearchBarCriterion, 1);
}


static void
option_menu_callback (GtkWidget *widget, gpointer data)
{
	NautilusSearchBarCriterion *criterion, *new_criterion;
	int type;
		
	criterion = (NautilusSearchBarCriterion *) data;
	
	type = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT(widget), "type"));

	new_criterion = nautilus_search_bar_criterion_next_new (type - 1);	

	criterion->details->callback (criterion, new_criterion, criterion->details->callback_data);
	
	nautilus_search_bar_criterion_destroy (criterion);
}


void                               
nautilus_search_bar_criterion_set_callback     (NautilusSearchBarCriterion *criterion,
						NautilusSearchBarCriterionCallback callback,
						gpointer data)
{
	criterion->details->callback = callback;
	criterion->details->callback_data = data;
}


static NautilusSearchBarCriterion *
nautilus_search_bar_criterion_new_from_values (NautilusSearchBarCriterionType type,
					       char *relation_options[],
					       gboolean use_value_entry,
					       gboolean use_value_menu,
					       char *value_options[])
{
	NautilusSearchBarCriterion *criterion;
	GtkWidget *search_criteria_option_menu, *search_criteria_menu; 
	GtkWidget *relation_option_menu, *relation_menu;
	GtkWidget *value_option_menu, *value_menu; 
	int i;
	
	criterion = nautilus_search_bar_criterion_new ();
	criterion->details = g_new0 (NautilusSearchBarCriterionDetails, 1);
	criterion->details->type = type;


	search_criteria_option_menu = gtk_option_menu_new ();
	search_criteria_menu = gtk_menu_new ();
	for (i = 0; criteria_titles[i] != NULL; i++) {
		GtkWidget *item;
		item = gtk_menu_item_new_with_label (_(criteria_titles[i]));
		gtk_object_set_data (GTK_OBJECT(item), "type", GINT_TO_POINTER(i));
		gtk_signal_connect (GTK_OBJECT (item), 
				    "activate",
				    option_menu_callback,
				    (gpointer) criterion);
		gtk_menu_append (GTK_MENU (search_criteria_menu),
				 item);
	}
	gtk_menu_set_active (GTK_MENU (search_criteria_menu), type);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (search_criteria_option_menu),
				  search_criteria_menu);
	criterion->details->available_criteria = GTK_OPTION_MENU (search_criteria_option_menu);
	gtk_widget_show_all (GTK_WIDGET(search_criteria_option_menu));



	relation_option_menu = gtk_option_menu_new ();
	relation_menu = gtk_menu_new ();
	for (i = 0; relation_options[i] != NULL; i++) {
		GtkWidget *item;
		item = gtk_menu_item_new_with_label (_(relation_options[i]));
		gtk_object_set_data (GTK_OBJECT(item), "type", GINT_TO_POINTER(i));
		gtk_menu_append (GTK_MENU (relation_menu),
				 item);
	}
	gtk_option_menu_set_menu (GTK_OPTION_MENU (relation_option_menu),
				  relation_menu);
	criterion->details->relation_menu = GTK_OPTION_MENU (relation_option_menu);
	gtk_widget_show_all (GTK_WIDGET(relation_option_menu));



	g_assert (! (use_value_entry && use_value_menu));
	criterion->details->use_value_entry = use_value_entry;
	if (use_value_entry) {
		criterion->details->value_entry = GTK_ENTRY (gtk_entry_new ());
		gtk_widget_show_all(GTK_WIDGET(criterion->details->value_entry));
	}
	criterion->details->use_value_menu = use_value_menu;
	if (use_value_menu) {
		g_return_val_if_fail (value_menu != NULL, NULL);
		value_option_menu = gtk_option_menu_new ();
		value_menu = gtk_menu_new ();
		for (i = 0; value_options[i] != NULL; i++) {
			GtkWidget *item;
			item = gtk_menu_item_new_with_label (_(value_options[i]));
			gtk_object_set_data (GTK_OBJECT(item), "type", GINT_TO_POINTER(i));
			gtk_menu_append (GTK_MENU (value_menu),
					 item);
		}
		gtk_option_menu_set_menu (GTK_OPTION_MENU (value_option_menu),
					  value_menu);
		criterion->details->value_menu = GTK_OPTION_MENU (value_option_menu);
		gtk_widget_show_all (GTK_WIDGET(criterion->details->value_menu));
	}

	return criterion;
}


NautilusSearchBarCriterion *
nautilus_search_bar_criterion_next_new (NautilusSearchBarCriterionType criterion_type)
{
	NautilusSearchBarCriterion *new_criterion;
	NautilusSearchBarCriterionType next_type;

	next_type = (criterion_type + 1) % NAUTILUS_LAST_CRITERION;

	switch(next_type) {
	case NAUTILUS_FILE_NAME_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_FILE_NAME_SEARCH_CRITERION,
									       name_relations,
									       TRUE,
									       FALSE,
									       NULL);
 		break; 
	case NAUTILUS_CONTENT_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_CONTENT_SEARCH_CRITERION,
									       content_relations,
									       TRUE,
									       FALSE,
									       NULL);
		break;
	case NAUTILUS_FILE_TYPE_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_FILE_TYPE_SEARCH_CRITERION,
									       type_relations,
									       FALSE,
									       TRUE,
									       type_objects);
		break;
	case NAUTILUS_LOCATION_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_LOCATION_SEARCH_CRITERION,
									       location_relations,
									       FALSE,
									       TRUE,
									       location_objects);
		break;
	case NAUTILUS_SIZE_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_SIZE_SEARCH_CRITERION,
									       size_relations,
									       FALSE,
									       TRUE,
									       size_objects);
		break;
	case NAUTILUS_EMBLEM_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_EMBLEM_SEARCH_CRITERION,
									       emblem_relations,
									       FALSE,
									       TRUE,
									       emblem_objects);
		break;
	case NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION,
									       modified_relations,
									       FALSE,
									       TRUE,
									       modified_objects);
		break;
	case NAUTILUS_OWNER_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_OWNER_SEARCH_CRITERION,
									       owner_relations,
									       TRUE,
									       FALSE,
									       NULL);
		break;
	default:
		g_assert_not_reached ();
	}
	
		
	return new_criterion;
}

NautilusSearchBarCriterion *
nautilus_search_bar_criterion_first_new (void)
{
	return nautilus_search_bar_criterion_new_from_values (NAUTILUS_FILE_NAME_SEARCH_CRITERION,
							      name_relations,
							      TRUE,
							      FALSE,
							      NULL); 	
}

/* returns a newly allocated string: needs to be freed by the caller. */
char *
nautilus_search_bar_criterion_get_location (NautilusSearchBarCriterion *criterion)
{
	GtkWidget *menu;
	GtkWidget *menu_item;
	int name_number, relation_number, value_number;
	char *value_text;
	
	/* There is ONE thing you should be aware of while implementing this function.
	   You have to make sure you use non-translated strings for building the uri.
	   So, to implement this, you are supposed to:
	   - build various tables which contain the strings corresponding to  a search type.
	   - call 
	          option_menu = gtk_option_menu_get_menu (criterion->details->some_menu)
		  menu_item = gtk_menu_get_active (optin_menu)
		  number = gtk_object_get_data (menu_item, "type")
	 */
	menu = gtk_option_menu_get_menu (criterion->details->available_criteria);
	menu_item = gtk_menu_get_active (GTK_MENU (menu));
	name_number = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (menu_item), "type"));

	menu = gtk_option_menu_get_menu (criterion->details->relation_menu);
	menu_item = gtk_menu_get_active (GTK_MENU (menu));
	relation_number = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (menu_item), "type"));

	if (criterion->details->use_value_menu) {
		menu = gtk_option_menu_get_menu (criterion->details->value_menu);
		menu_item = gtk_menu_get_active (GTK_MENU (menu));
		value_number = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (menu_item), "type"));
	}
	else {
		value_text = gtk_entry_get_text (GTK_ENTRY (criterion->details->value_entry));
	}

	switch (name_number) {
	case NAUTILUS_FILE_NAME_SEARCH_CRITERION:
		return get_name_location_for (relation_number,
					       value_text);
	case NAUTILUS_CONTENT_SEARCH_CRITERION:
		return get_content_location_for (relation_number,
						 value_text);
	case NAUTILUS_FILE_TYPE_SEARCH_CRITERION:
		return get_file_type_location_for (relation_number,
						   value_number);
	case NAUTILUS_SIZE_SEARCH_CRITERION:
		return get_size_location_for (relation_number,
					      value_text);
	case NAUTILUS_EMBLEM_SEARCH_CRITERION:
		return get_emblem_location_for (relation_number,
						value_number);
	case NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION:
		return get_date_modified_location_for (relation_number,
						       value_number);
	case NAUTILUS_OWNER_SEARCH_CRITERION:
		return get_owner_location_for (relation_number,
					       value_text);
	default:
		g_assert_not_reached ();
		return NULL;
	}
	return g_strdup ("file_name contains evolution ");
}


NautilusSearchBarCriterionType
get_next_default_search_criterion_type (NautilusSearchBarCriterionType type) 
{
	switch (type) {
	case NAUTILUS_FILE_NAME_SEARCH_CRITERION:
		return NAUTILUS_CONTENT_SEARCH_CRITERION;
	case NAUTILUS_CONTENT_SEARCH_CRITERION:
		return NAUTILUS_FILE_TYPE_SEARCH_CRITERION;
	case NAUTILUS_FILE_TYPE_SEARCH_CRITERION:
		return NAUTILUS_LOCATION_SEARCH_CRITERION;
	case NAUTILUS_LOCATION_SEARCH_CRITERION:
		return NAUTILUS_SIZE_SEARCH_CRITERION;
	case NAUTILUS_SIZE_SEARCH_CRITERION:
		return NAUTILUS_SIZE_SEARCH_CRITERION;
	case NAUTILUS_EMBLEM_SEARCH_CRITERION:
		return NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION;
	case NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION:
		return NAUTILUS_OWNER_SEARCH_CRITERION;
	case NAUTILUS_OWNER_SEARCH_CRITERION:
		return NAUTILUS_FILE_NAME_SEARCH_CRITERION;
	default:
		g_assert_not_reached ();
		return NAUTILUS_LAST_CRITERION;
	}
	return NAUTILUS_LAST_CRITERION;
}

void                               
nautilus_search_bar_criterion_show (NautilusSearchBarCriterion *criterion)
{
	
	gtk_widget_show (GTK_WIDGET (criterion->details->available_criteria));
	gtk_widget_show (GTK_WIDGET (criterion->details->relation_menu));

	if (criterion->details->use_value_entry) {
		gtk_widget_show (GTK_WIDGET (criterion->details->value_entry));
	}
	if (criterion->details->use_value_menu) {
		gtk_widget_show (GTK_WIDGET (criterion->details->value_menu));
	}
}



void                               
nautilus_search_bar_criterion_hide (NautilusSearchBarCriterion *criterion)
{
	
	gtk_widget_hide (GTK_WIDGET (criterion->details->available_criteria));
	gtk_widget_hide (GTK_WIDGET (criterion->details->relation_menu));

	if (criterion->details->use_value_entry) {
		gtk_widget_hide (GTK_WIDGET (criterion->details->value_entry));
	}
	if (criterion->details->use_value_menu) {
		gtk_widget_hide (GTK_WIDGET (criterion->details->value_menu));
	}
}

char *
nautilus_search_bar_criterion_human_from_uri (const char *location_uri)
{
	


	return g_strdup (location_uri);


}

static char *                              
get_name_location_for (int relation_number, char *name_text)
{
	const char *possible_relations[] = { "contains",
					    "starts_with",
					    "ends_with",
					    "matches_glob",
					    "matches_regexp" };
	
	g_assert (relation_number >= 0);
	g_assert (relation_number < 5);

	return g_strdup_printf ("file_name %s %s", possible_relations[relation_number], 
				name_text);
	
}

static char *                              
get_content_location_for (int relation_number, char *name_text)
{
	const char *possible_relations[] = { "includes",
					    "does_not_include" };
	
	g_assert (relation_number == 0 || relation_number == 1);

	return g_strdup_printf ("content %s %s", possible_relations[relation_number],
				name_text);
}

static char *                              
get_file_type_location_for (int relation_number,
			    int value_number)
{
	const char *possible_relations[] = { "is", "is_not" };
	const char *possible_values[] = {"file", "text_file", "application", "directory", "music" };

	g_assert (relation_number == 0 || relation_number == 1);
	g_assert (value_number >= 0);
	g_assert (value_number < 5);
	return g_strdup_printf ("file_type %s %s", possible_relations[relation_number],
				possible_values[value_number]);
}


static char *                              
get_size_location_for (int relation_number,
		       char *size_text)
{
	const char *possible_relations[] = { "larger_than", "smaller_than" };
	
	g_assert (relation_number == 0 || relation_number == 1);
	/* FIXME:  Need checks for appropriate size here */
	return g_strdup_printf ("size %s %s", possible_relations[relation_number], size_text);

}

static char *                              
get_emblem_location_for  (int relation_number,
			  int value_number)
{
	/* FIXME */
	return g_strdup ("");
}

static char *                              
get_date_modified_location_for (int relation_number,
				int value_number)
{
	const char *possible_relations[] = { "updated", "not_updated" };
	const char *possible_values[] = { "today", "this_week", "this_month" };

	g_assert (relation_number == 0 || relation_number == 1);
	g_assert (value_number >= 0);
	g_assert (value_number < 3);

	return g_strdup_printf ("mod_time %s %s", possible_relations[relation_number],
				possible_values[value_number]);

}

static char *                              
get_owner_location_for (int relation_number,
			char *owner_text)
{
	const char *possible_relations[] = { "is", "is not" };
	g_assert (relation_number == 0 || relation_number == 1);
	return g_strdup_printf ("owner %s %s", possible_relations[relation_number], owner_text);
	
}











