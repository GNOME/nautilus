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

#include <libnautilus-extensions/nautilus-customization-data.h>
#include <libnautilus-extensions/nautilus-dateedit-extensions.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include "nautilus-search-bar-criterion.h"
#include "nautilus-search-bar-criterion-private.h"
#include "nautilus-signaller.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <gtk/gtksignal.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include <libgnomeui/gnome-dateedit.h>
#include <libgnomeui/gnome-uidefs.h>

static char * criteria_titles [] = {
	  N_("Name"),
	  N_("Content"),
	  N_("Type"),
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
	N_("includes all of"),
	N_("includes any of"),
	N_("does not include all of"),
	N_("includes none of"),
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

static char *size_relations [] = {
	N_("larger than"),
	N_("smaller than"),
	NULL
};

static char *emblem_relations [] = {
	N_("marked with"),
	N_("not marked with"),
	NULL
};

static char *modified_relations [] = {
	N_("is"),
	N_("is not"),
	N_("is after"),
	N_("is before"),
	N_("--"),
	N_("is today"),
	N_("is yesterday"),
	N_("--"),
	N_("is within a week of"),
	N_("is within a month of"),
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
										    char *value_options[],
										    gboolean use_value_suffix,
										    char *value_suffix);


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
										   GtkWidget *menu_item);
static char *                              get_date_modified_location_for         (int relation_number,
										   char *date);
static char *                              get_owner_location_for                 (int relation_number,
										   char *owner_number);
static void                                make_emblem_value_menu                 (NautilusSearchBarCriterion *criterion);

static void                                emblems_changed_callback               (GtkObject *signaller,
										   gpointer data);

void
nautilus_search_bar_criterion_destroy (NautilusSearchBarCriterion *criterion)
{
	/* FIXME bugzilla.eazel.com 2437: need more freeage */
	gtk_signal_disconnect_by_data (GTK_OBJECT (nautilus_signaller_get_current ()),
				       criterion);
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
					       char *value_options[],
					       gboolean use_value_suffix,
					       char *value_suffix)
{
	NautilusSearchBarCriterion *criterion;
	GtkWidget *search_criteria_option_menu, *search_criteria_menu; 
	GtkWidget *relation_option_menu, *relation_menu;
	GtkWidget *value_option_menu, *value_menu; 

	int i;
	
	criterion = nautilus_search_bar_criterion_new ();
	criterion->details = g_new0 (NautilusSearchBarCriterionDetails, 1);
	criterion->details->type = type;

	gtk_signal_connect (GTK_OBJECT (nautilus_signaller_get_current ()),
			    "emblems_changed",
			    emblems_changed_callback,
			    (gpointer) criterion);
				       

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
	if (use_value_menu && criterion->details->type != NAUTILUS_EMBLEM_SEARCH_CRITERION) {
		g_return_val_if_fail (value_options != NULL, NULL);
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
	criterion->details->use_value_suffix = use_value_suffix;
	if (use_value_suffix) {
		g_return_val_if_fail (value_suffix != NULL, NULL);
		criterion->details->value_suffix = GTK_LABEL (gtk_label_new (value_suffix));
		gtk_widget_show_all (GTK_WIDGET (criterion->details->value_suffix));
	}
	/* Special case widgets here */
	if (criterion->details->type == NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION) {
		criterion->details->date = GNOME_DATE_EDIT (gnome_date_edit_new (time (NULL), FALSE, FALSE));
		gtk_widget_show_all (GTK_WIDGET (criterion->details->date));
	}
	if (criterion->details->type == NAUTILUS_EMBLEM_SEARCH_CRITERION) {
		value_option_menu = gtk_option_menu_new ();
		criterion->details->value_menu = GTK_OPTION_MENU (value_option_menu);
		make_emblem_value_menu (criterion);
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
									       NULL,
									       FALSE,
									       NULL);
 		break; 
	case NAUTILUS_CONTENT_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_CONTENT_SEARCH_CRITERION,
									       content_relations,
									       TRUE,
									       FALSE,
									       NULL,
									       FALSE,
									       NULL);
		break;
	case NAUTILUS_FILE_TYPE_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_FILE_TYPE_SEARCH_CRITERION,
									       type_relations,
									       FALSE,
									       TRUE,
									       type_objects,
									       FALSE,
									       NULL);
		break;
	case NAUTILUS_SIZE_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_SIZE_SEARCH_CRITERION,
									       size_relations,
									       TRUE,
									       FALSE,
									       NULL,
									       TRUE,
									       "K");
		break;
	case NAUTILUS_EMBLEM_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_EMBLEM_SEARCH_CRITERION,
									       emblem_relations,
									       FALSE,
									       FALSE,
									       NULL,
									       FALSE,
									       NULL);
									       
		break;
	case NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION,
									       modified_relations,
									       FALSE,
									       FALSE,
									       NULL,
									       FALSE,
									       NULL);
		break;
	case NAUTILUS_OWNER_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_OWNER_SEARCH_CRITERION,
									       owner_relations,
									       TRUE,
									       FALSE,
									       NULL,
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
							      NULL,
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
	else if (criterion->details->use_value_entry) {
		value_text = gtk_entry_get_text (GTK_ENTRY (criterion->details->value_entry));
	}
	else if (criterion->details->type == NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION) {
		value_text = nautilus_gnome_date_edit_get_date_as_string (criterion->details->date);
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
						menu_item);
	case NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION:
		return get_date_modified_location_for (relation_number,
						       value_text);
	case NAUTILUS_OWNER_SEARCH_CRITERION:
		return get_owner_location_for (relation_number,
					       value_text);
	default:
		g_assert_not_reached ();
		return NULL;
	}

	g_assert_not_reached ();
	return NULL;
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

char *
nautilus_search_uri_get_first_criterion (const char *search_uri)
{
	char *unescaped_uri;
	char *first_criterion;
	int matches;

	unescaped_uri = gnome_vfs_unescape_string (search_uri, NULL);

	/* Start with a string as long as the URI, since
	 * we don't necessarily trust the search_uri to have
	 * the pattern we're looking for.
	 */
	first_criterion = g_strdup (unescaped_uri);

	matches = sscanf (unescaped_uri, "search:[file:///]%s %*s", first_criterion);
	g_free (unescaped_uri);
	
	if (matches == 0) {
		g_free (first_criterion);
		return NULL;
	}

	return first_criterion;
}


static char *                              
get_name_location_for (int relation_number, char *name_text)
{
	const char *possible_relations[] = { "contains",
					    "begins_with",
					    "ends_with",
					    "matches",
					    "matches_regexp" };
	
	g_assert (relation_number >= 0);
	g_assert (relation_number < 5);

	return g_strdup_printf ("%s %s %s", NAUTILUS_SEARCH_URI_TEXT_NAME,
				possible_relations[relation_number], 
				name_text);
	
}

static char *                              
get_content_location_for (int relation_number, char *name_text)
{
	const char *possible_relations[] = { "includes_all_of",
					     "includes_any_of",
					     "does_not_include_all_of",
					     "does_not_include_any_of" };
	
	g_assert (relation_number>= 0);
	g_assert (relation_number < 4);

	return g_strdup_printf ("%s %s %s", NAUTILUS_SEARCH_URI_TEXT_CONTENT, 
				possible_relations[relation_number],
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
	return g_strdup_printf ("%s %s %s", NAUTILUS_SEARCH_URI_TEXT_TYPE, 
				possible_relations[relation_number],
				possible_values[value_number]);
}


static char *                              
get_size_location_for (int relation_number,
		       char *size_text)
{
	const char *possible_relations[] = { "larger_than", "smaller_than" };
	int entered_size;
	
	g_assert (relation_number == 0 || relation_number == 1);
	/* We put a 'K' after the size, so multiply what the user
	   entered by 1000 */
	entered_size = strtol (size_text, NULL, 10);
	/* FIXME bugzilla.eazel.com 2438:  Need error handling here */
	g_return_val_if_fail (entered_size >= 0, NULL);
	return g_strdup_printf ("%s %s %d", NAUTILUS_SEARCH_URI_TEXT_SIZE, 
				possible_relations[relation_number], 
				entered_size * 1000);

}

static char *                              
get_emblem_location_for  (int relation_number,
			  GtkWidget *menu_item)
{
	const char *possible_relations[] = {"include", "does_not_include" };
	char *emblem_text;
	
	g_assert (relation_number == 0 ||
		  relation_number == 1);
	emblem_text = gtk_object_get_data (GTK_OBJECT (menu_item),
					   "emblem name");
        return g_strdup_printf ("%s %s %s", NAUTILUS_SEARCH_URI_TEXT_EMBLEMS,
				possible_relations[relation_number],
				emblem_text);
}

static char *                              
get_date_modified_location_for (int relation_number,
				char *date_string)
{
	const char *possible_relations[] = { "is", 
					     "is_not", 
					     "is_after", 
					     "is_before", 
					     "--", 
					     "is_today",
					     "is_yesterday",
					     "--",
					     "is_within_a_week_of", 
					     "is_within_a_month_of" };
	char *result;

	g_assert (relation_number >= 0);
	g_assert (relation_number < 10);
	g_return_val_if_fail (relation_number != 4 && relation_number != 7, g_strdup (""));
	
	/* Handle "is today" and "is yesterday" separately */
	if (relation_number == 5) {
		result = g_strdup_printf ("%s is today", NAUTILUS_SEARCH_URI_TEXT_DATE_MODIFIED);
	}
	if (relation_number == 6) {
		result = g_strdup_printf ("%s is yesterday", NAUTILUS_SEARCH_URI_TEXT_DATE_MODIFIED);
	}
	if (relation_number != 5 && relation_number != 6) {
		if (date_string == NULL) {
			return g_strdup ("");
		}
		else {
			result = g_strdup_printf ("%s %s %s", NAUTILUS_SEARCH_URI_TEXT_DATE_MODIFIED,
						  possible_relations[relation_number],
						  date_string);
		}
	}
	return result;
	
}

static char *                              
get_owner_location_for (int relation_number,
			char *owner_text)
{
	const char *possible_relations[] = { "is", "is not" };
	g_assert (relation_number == 0 || relation_number == 1);
	return g_strdup_printf ("%s %s %s", NAUTILUS_SEARCH_URI_TEXT_OWNER, possible_relations[relation_number], owner_text);
	
}

static void                                
make_emblem_value_menu (NautilusSearchBarCriterion *criterion)
{
	NautilusCustomizationData *customization_data;
	GtkWidget *temp_hbox;
	GtkWidget *menu_item;
	char *emblem_file_name;
	char *emblem_display_name;
	GtkWidget *emblem_pixmap_widget;
	GtkLabel *emblem_label;
	GtkWidget *value_menu; 
	
	/* Add the items to the emblems menu here */
	/* FIXME bugzilla.eazel.com 2738: What are the variables for thumbnail icon height and width */

	value_menu = gtk_menu_new ();
	customization_data = nautilus_customization_data_new ("emblems",
							      TRUE,
							      NAUTILUS_ICON_SIZE_FOR_MENUS, 
							      NAUTILUS_ICON_SIZE_FOR_MENUS);
	while (nautilus_customization_data_get_next_element_for_display (customization_data,
									 &emblem_file_name,
									 &emblem_pixmap_widget,
									 &emblem_label) == GNOME_VFS_OK) {
		
		menu_item = gtk_menu_item_new ();
		gtk_label_get (emblem_label, &emblem_display_name);
		gtk_object_set_data (GTK_OBJECT (menu_item), "emblem name", emblem_display_name);
		temp_hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
		gtk_container_add (GTK_CONTAINER (menu_item), temp_hbox);
		gtk_box_pack_start (GTK_BOX (temp_hbox), emblem_pixmap_widget, FALSE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX (temp_hbox), GTK_WIDGET (emblem_label), FALSE, FALSE, 0);
		gtk_menu_append (GTK_MENU (value_menu), menu_item);
	}

				      
	gtk_option_menu_set_menu (criterion->details->value_menu,
				  value_menu);
	gtk_widget_show_all (GTK_WIDGET (criterion->details->value_menu));
	criterion->details->use_value_menu = TRUE;
	g_free (emblem_file_name);
}



static void
emblems_changed_callback (GtkObject *signaller,
			  gpointer data)
{
	NautilusSearchBarCriterion *criterion;
	GtkWidget *menu_widget;

	/* FIXME bugzilla.eazel.com 2739: check type here in some way */
	criterion = NAUTILUS_SEARCH_BAR_CRITERION (data);

	if (criterion->details->type == NAUTILUS_EMBLEM_SEARCH_CRITERION) {
		/* Get rid of the old menu */
		menu_widget = gtk_option_menu_get_menu (criterion->details->value_menu);
		gtk_option_menu_remove_menu (criterion->details->value_menu);
		gtk_widget_destroy (menu_widget);
		make_emblem_value_menu (criterion);
	}
}




