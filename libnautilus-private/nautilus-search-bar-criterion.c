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


#include <gtk/gtkentry.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

#include <libgnomeui/gnome-uidefs.h>


static char *criteria_titles[] = {
	_("Name"),
	_("Content"),
	_("Type"),
	_("Stored"),
	_("Size"),
	_("With Note"),
	_("With Emblem"),
	_("Last Modified"),
	_("Owned By"),
	NULL
};

static char *name_relations [] = {
	_("contains"),
	_("starts with"),
	_("ends with"),
	_("matches glob"),
	_("matches regexp"),
	NULL
};

static char *content_relations [] = {
	_("includes"),
	_("does not include"),
	NULL

};

static char *type_relations [] = {
	_("is"),
	_("is not"),
	NULL
};

static char *type_options [] = {
	_("regular file"),
	_("text file"),
	_("application"),
	_("directory"),
	_("music"),
	NULL
};

static char *location_relations [] = {
	_("is"),
	NULL
};

static char *location_options [] = {
	_("on this computer"),
	_("in my vault"),
	NULL
};

static char *size_relations [] = {
	_("larger than"),
	_("smaller than"),
	NULL
};

static char *notes_relations [] = {
	_("including"),
	_("not including"),
	NULL
};

static char *emblem_relations [] = {
	_("marked with"),
	_("not marked with"),
	NULL
};

static char *emblem_options [] = {
	/* FIXME: add emblem possibilities here */
	NULL
};
	
static char *modified_relations [] = {
	_("after"),
	_("before"),
	NULL
};

static char *modified_options [] = {
	_("today"),
	_("this week"),
	_("this month"),
	NULL
};

static char *owner_relations [] = {
	_("is"),
	_("is not"),
	NULL
};

static NautilusSearchBarCriterion * nautilus_search_bar_criterion_new              (void);

static NautilusSearchBarCriterion * nautilus_search_bar_criterion_new_from_values  (NautilusSearchBarCriterionType type,
										    char *operator_options[],
										    gboolean use_value_entry,
										    gboolean use_value_menu,
										    char *value_options[]);

NautilusSearchBarCriterionType      get_next_default_search_criterion_type         (NautilusSearchBarCriterionType type) ;

void
nautilus_search_bar_criterion_destroy (NautilusSearchBarCriterion *criterion)
{
	/* FIXME : need more freeage */
	g_free (criterion->details);
}

static NautilusSearchBarCriterion * 
nautilus_search_bar_criterion_new (void)
{
	return g_new0 (NautilusSearchBarCriterion, 1);
}

static NautilusSearchBarCriterion *
nautilus_search_bar_criterion_new_from_values (NautilusSearchBarCriterionType type,
					       char *operator_options[],
					       gboolean use_value_entry,
					       gboolean use_value_menu,
					       char *value_options[])
{
	NautilusSearchBarCriterion *criterion;
	GtkWidget *search_criteria_option_menu, *search_criteria_menu; 
	GtkWidget *operator_option_menu, *operator_menu;
	GtkWidget *value_option_menu, *value_menu; 
	int i;
	
	criterion = nautilus_search_bar_criterion_new ();
	criterion->details = g_new0 (NautilusSearchBarCriterionDetails, 1);
	criterion->details->type = type;


	search_criteria_option_menu = gtk_option_menu_new ();
	search_criteria_menu = gtk_menu_new ();
	for (i = 0; criteria_titles[i] != NULL; i++) {
		gtk_menu_append (GTK_MENU (search_criteria_menu),
				 gtk_menu_item_new_with_label (criteria_titles[i]));
	}
	gtk_option_menu_set_menu (GTK_OPTION_MENU (search_criteria_option_menu),
				  search_criteria_menu);
	gtk_menu_set_active (GTK_MENU (search_criteria_menu), type);
	criterion->details->available_option_menu = GTK_OPTION_MENU (search_criteria_option_menu);
	g_return_val_if_fail (operator_menu != NULL, NULL);
	
	operator_option_menu = gtk_option_menu_new ();
	operator_menu = gtk_menu_new ();
	for (i = 0; operator_options[i] != NULL; i++) {
		gtk_menu_append (GTK_MENU (operator_menu),
				 gtk_menu_item_new_with_label (operator_options[i]));
	}
	gtk_option_menu_set_menu (GTK_OPTION_MENU (operator_option_menu),
				  operator_menu);
	criterion->details->operator_menu = GTK_OPTION_MENU (operator_option_menu);

	g_assert (! (use_value_entry && use_value_menu));
	criterion->details->use_value_entry = use_value_entry;
	if (use_value_entry) {
		criterion->details->value_entry = GTK_ENTRY (gtk_entry_new ());
	}
	criterion->details->use_value_menu = use_value_menu;
	if (use_value_menu) {
		g_return_val_if_fail (value_menu != NULL, NULL);
		value_option_menu = gtk_option_menu_new ();
		value_menu = gtk_menu_new ();
		for (i = 0; value_options[i] != NULL; i++) {
			gtk_menu_append (GTK_MENU (value_menu),
					 gtk_menu_item_new_with_label (value_options[i]));
		}
		gtk_option_menu_set_menu (GTK_OPTION_MENU (value_option_menu),
					  value_menu);
		criterion->details->value_menu = GTK_OPTION_MENU (value_option_menu);

	}

	return criterion;
}


NautilusSearchBarCriterionList *
nautilus_search_bar_criterion_next_new (NautilusSearchBarCriterionList *criteria)
{
	NautilusSearchBarCriterion *last_criterion;
	NautilusSearchBarCriterion *new_criterion;
	NautilusSearchBarCriterionType next_type;

	if (criteria == NULL) {
		return g_list_append (NULL,
				      nautilus_search_bar_criterion_first_new ());
	}
	last_criterion = nautilus_search_bar_criterion_list_get_last (criteria);
	next_type = get_next_default_search_criterion_type (last_criterion->details->type);

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
									       type_options);
		break;
	case NAUTILUS_LOCATION_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_LOCATION_SEARCH_CRITERION,
									       location_relations,
									       FALSE,
									       TRUE,
									       location_options);
		break;
	case NAUTILUS_SIZE_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_SIZE_SEARCH_CRITERION,
									       size_relations,
									       TRUE,
									       FALSE,
									       NULL);
		break;
	case NAUTILUS_NOTES_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_NOTES_SEARCH_CRITERION,
									       notes_relations,
									       TRUE,
									       FALSE,
									       NULL);
		break;
	case NAUTILUS_EMBLEM_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_EMBLEM_SEARCH_CRITERION,
									       emblem_relations,
									       FALSE,
									       TRUE,
									       emblem_options);
		break;
	case NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION,
									       modified_relations,
									       FALSE,
									       TRUE,
									       modified_options);
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
	
		

	return g_list_append (criteria, new_criterion);
	
}

NautilusSearchBarCriterion *
nautilus_search_bar_criterion_first_new ()
{
	return nautilus_search_bar_criterion_new_from_values (NAUTILUS_FILE_NAME_SEARCH_CRITERION,
							      name_relations,
							      TRUE,
							      FALSE,
							      NULL);
 	
}


NautilusSearchBarCriterion *
nautilus_search_bar_criterion_list_get_last (NautilusSearchBarCriterionList *criteria)
{
	NautilusSearchBarCriterionList *end_of_criteria;
	NautilusSearchBarCriterion *last_criterion;

	g_return_val_if_fail (criteria != NULL, NULL);

	end_of_criteria = g_list_last (criteria);
	last_criterion = NAUTILUS_SEARCH_BAR_CRITERION (end_of_criteria->data);

	return last_criterion;
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
		return NAUTILUS_NOTES_SEARCH_CRITERION;
	case NAUTILUS_NOTES_SEARCH_CRITERION:
		return NAUTILUS_EMBLEM_SEARCH_CRITERION;
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
	
	gtk_widget_show (GTK_WIDGET (criterion->details->available_option_menu));
	gtk_widget_show (GTK_WIDGET (criterion->details->operator_menu));

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
	
	gtk_widget_hide (GTK_WIDGET (criterion->details->available_option_menu));
	gtk_widget_hide (GTK_WIDGET (criterion->details->operator_menu));

	if (criterion->details->use_value_entry) {
		gtk_widget_hide (GTK_WIDGET (criterion->details->value_entry));
	}
	if (criterion->details->use_value_menu) {
		gtk_widget_hide (GTK_WIDGET (criterion->details->value_menu));
	}
}





