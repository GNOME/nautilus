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

#include <config.h>
#include "nautilus-search-bar-criterion.h"

#include "nautilus-search-bar-criterion-private.h"
#include "nautilus-signaller.h"
#include <eel/eel-dateedit-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-labeled-image.h>
#include <eel/eel-string.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dateedit.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-private/nautilus-customization-data.h>
#include <libnautilus-private/nautilus-entry.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-search-uri.h>
#include <libnautilus-private/nautilus-undo-signal-handlers.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "gtkhwrapbox.h"

enum {
	CRITERION_TYPE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static char * criteria_titles [] = { 
         /* Menu item in the search bar.
            Bracketed items are context, and are message
            strings elsewhere.  You don't have to translate the whole
            string, and only the translation for "containing '%s' will
            be used.  If you do translate the whole string, leave the
            translations of the rest of the text in brackets, so it
            will not be used.  */
        N_("[Search for] Name [contains \"fish\"]"),
        N_("[Search for] Content [includes all of \"fish tree\"]"),
        N_("[Search for] Type [is regular file]"),
        N_("[Search for] Size [larger than 400K]"),
	N_("[Search for] With Emblem [includes \"Important\"]"),
	N_("[Search for] Last Modified [before yesterday]"),
	N_("[Search for] Owner [is not root]"),
	NULL
};


static char *name_relations [] = {
	N_("[File name] contains [help]"),
	N_("[File name] starts with [nautilus]"),
	N_("[File name] ends with [.c]"),
	N_("[File name] matches glob [*.c]"),
	N_("[File name] matches regexp [\"e??l.$\"]"),
	NULL
};

static char *content_relations [] = {
	N_("[File content] includes all of [apple orange]"),
	N_("[File content] includes any of [apply orange]"),
	N_("[File content] does not include all of [apple orange]"),
	N_("[File content] includes none of [apple orange]"),
	NULL

};

static char *type_relations [] = {
	N_("[File type] is [folder]"),
	N_("[File type] is not [folder]"),
	NULL
};

static char *type_objects [] = {
	N_("[File type is] regular file"),
	N_("[File type is] text file"),
	N_("[File type is] application"),
	N_("[File type is] folder"),
	N_("[File type is] music"),
	NULL
};

static char *size_relations [] = {
	N_("[File size is] larger than [400K]"),
	N_("[File size is] smaller than [300K]"),
	NULL
};

static char *emblem_relations [] = {
	N_("[With emblem] marked with [Important]"),
	N_("[With emblem] not marked with [Important]"),
	NULL
};

static char *modified_relations [] = {
	N_("[Last modified date] is [1/24/00]"),
	N_("[Last modified date] is not [1/24/00]"),
	N_("[Last modified date] is after [1/24/00]"),
	N_("[Last modified date] is before [1/24/00]"),
	"",
	N_("[Last modified date] is today"),
	N_("[Last modified date] is yesterday"),
	"",
	N_("[Last modified date] is within a week of [1/24/00]"),
	N_("[Last modified date] is within a month of [1/24/00]"),
	NULL
};

static gboolean modified_relation_shows_date [] = {
	TRUE,
	TRUE,
	TRUE,
	TRUE, 
	TRUE,     /* Separator */
	FALSE,    /* is today */
	FALSE,    /* is yesterday */
	TRUE,     /* Separator */
	TRUE,
	TRUE,
	/* NULL */
};

static char *owner_relations [] = {
	N_("[File owner] is [root]"),
	N_("[File owner] is not [root]"),
	NULL
};


static GtkWidget *                  nautilus_search_bar_criterion_new              (void);
static NautilusSearchBarCriterion * nautilus_search_bar_criterion_new_from_values  (NautilusSearchBarCriterionType type,
										    char *operator_options[],
										    gboolean use_value_entry,
										    gboolean use_value_menu,
										    char *value_options[],
										    gboolean use_value_suffix,
										    char *value_suffix,
										    NautilusComplexSearchBar *bar);


NautilusSearchBarCriterionType      get_next_default_search_criterion_type         (NautilusSearchBarCriterionType type) ;
static void                         nautilus_search_bar_criterion_initialize_class (NautilusSearchBarCriterionClass *klass);
static void                         nautilus_search_bar_criterion_initialize       (NautilusSearchBarCriterion *criterion);
static gboolean                     modified_relation_should_show_value            (int relation_index);
static void                         hide_date_widget                               (GtkObject *object,
										    gpointer data);
static void                         show_date_widget                               (GtkObject *object,
										    gpointer data);
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

static void                                criterion_type_changed_callback        (GtkObject *object,
										   gpointer data);
static void                                emblems_changed_callback               (GtkObject *signaller,
										   gpointer data);
static gboolean                            criterion_type_already_is_displayed         (GSList *criteria,
											NautilusSearchBarCriterionType criterion_number);
static NautilusSearchBarCriterionType      get_next_criterion_type                (NautilusSearchBarCriterionType current_type,
										   GSList *displayed_criteria);
static void                                nautilus_search_bar_criterion_destroy  (GtkObject *object);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusSearchBarCriterion, nautilus_search_bar_criterion, GTK_TYPE_EVENT_BOX)

     static void
nautilus_search_bar_criterion_initialize_class (NautilusSearchBarCriterionClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = nautilus_search_bar_criterion_destroy;
	
	signals[CRITERION_TYPE_CHANGED] = gtk_signal_new
		("criterion_type_changed",
		 GTK_RUN_LAST,
		 object_class->type,
		 0,
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
				
}

void
nautilus_search_bar_criterion_destroy (GtkObject *object)
{
	NautilusSearchBarCriterion *criterion;

	g_return_if_fail (NAUTILUS_IS_SEARCH_BAR_CRITERION (object));
	criterion = NAUTILUS_SEARCH_BAR_CRITERION (object);

	/* FIXME bugzilla.gnome.org 42437: need more freeage */
	gtk_signal_disconnect_by_data (nautilus_signaller_get_current (),
				       criterion);
	/*	nautilus_undo_editable_set_undo_key (GTK_EDITABLE (criterion->details->value_entry), FALSE);
		nautilus_undo_tear_down_nautilus_entry_for_undo (criterion->details->value_entry);
	*/
	g_free (criterion->details);
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}


static GtkWidget *
nautilus_search_bar_criterion_new (void)
{
	return gtk_widget_new (NAUTILUS_TYPE_SEARCH_BAR_CRITERION, NULL);
}


static void
nautilus_search_bar_criterion_initialize (NautilusSearchBarCriterion *criterion)
{
	criterion->details = g_new0 (NautilusSearchBarCriterionDetails, 1);
}

static void
queue_bar_resize_callback (GtkObject *wrap_box,
			   gpointer bar)
{
	NautilusComplexSearchBar *complex_bar;

	complex_bar = NAUTILUS_COMPLEX_SEARCH_BAR (bar);
	if (GTK_WIDGET_VISIBLE (wrap_box)) {
		nautilus_complex_search_bar_queue_resize (complex_bar);
	}
}

static NautilusSearchBarCriterion *
nautilus_search_bar_criterion_new_from_values (NautilusSearchBarCriterionType type,
					       char *relation_options[],
					       gboolean use_value_entry,
					       gboolean use_value_menu,
					       char *value_options[],
					       gboolean use_value_suffix,
					       char *value_suffix,
					       NautilusComplexSearchBar *bar)
{
	NautilusSearchBarCriterion *criterion;
	NautilusSearchBarCriterionDetails *details;
	GtkWidget *search_criteria_option_menu, *search_criteria_menu; 
	GtkWidget *relation_option_menu, *relation_menu;
	GtkWidget *value_option_menu, *value_menu; 
	char *context_stripped_criteria_title;
	char *context_stripped_relation, *context_stripped_value;
	int i;
	
	g_return_val_if_fail (NAUTILUS_IS_COMPLEX_SEARCH_BAR (bar), NULL);

	criterion = NAUTILUS_SEARCH_BAR_CRITERION (nautilus_search_bar_criterion_new ());
	details = criterion->details;

	details->type = type;
	details->bar = bar;
	details->box = gtk_hwrap_box_new (FALSE);

	gtk_signal_connect (GTK_OBJECT (details->box),
			    "need_reallocation",
			    queue_bar_resize_callback,
			    bar);
	gtk_signal_connect (nautilus_signaller_get_current (),
			    "emblems_changed",
			    emblems_changed_callback,
			    (gpointer) criterion);
	

	search_criteria_option_menu = gtk_option_menu_new ();
	search_criteria_menu = gtk_menu_new ();

	for (i = 0; criteria_titles[i] != NULL; i++) {
		GtkWidget *item;
		context_stripped_criteria_title = eel_str_remove_bracketed_text (_(criteria_titles[i]));
		item = gtk_menu_item_new_with_label (context_stripped_criteria_title);
		g_free (context_stripped_criteria_title);

		gtk_object_set_data (GTK_OBJECT(item), "type", GINT_TO_POINTER(i));
		gtk_signal_connect (GTK_OBJECT (item),
				    "activate",
				    criterion_type_changed_callback,
				    (gpointer) criterion);
		gtk_menu_append (GTK_MENU (search_criteria_menu),
				 item);
		gtk_widget_show (item);
	}
	gtk_option_menu_set_menu (GTK_OPTION_MENU (search_criteria_option_menu),
				  search_criteria_menu);
	details->available_criteria = GTK_OPTION_MENU (search_criteria_option_menu);
	gtk_widget_show_all (GTK_WIDGET (search_criteria_option_menu));
	gtk_wrap_box_pack (GTK_WRAP_BOX (details->box),
			   GTK_WIDGET (details->available_criteria),
			   FALSE,
			   FALSE,
			   FALSE,
			   FALSE);
	
	relation_option_menu = gtk_option_menu_new ();
	relation_menu = gtk_menu_new ();
	for (i = 0; relation_options[i] != NULL; i++) {
		GtkWidget *item;
		if (eel_str_is_empty (relation_options[i])) {
			/* Empty text; make unselectable separator */
			item = gtk_menu_item_new ();
			gtk_widget_set_sensitive (item, FALSE);
		} else {
			context_stripped_relation = eel_str_remove_bracketed_text (_(relation_options[i]));
			item = gtk_menu_item_new_with_label (context_stripped_relation);
			g_free (context_stripped_relation);
		}
		gtk_widget_show (item);
		gtk_object_set_data (GTK_OBJECT(item), "type", GINT_TO_POINTER(i));
		/* Callback to desensitize the date widget for menu items that
		   don't need a date, like "yesterday" */
		if (details->type == NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION) {
			if (modified_relation_should_show_value (i)) {
				gtk_signal_connect (GTK_OBJECT (item), "activate",
						    show_date_widget,
						    criterion);
			}
			else {
				gtk_signal_connect (GTK_OBJECT (item), "activate",
						    hide_date_widget,
						    criterion);
			}
		}
		gtk_menu_append (GTK_MENU (relation_menu),
				 item);
	}
	gtk_option_menu_set_menu (GTK_OPTION_MENU (relation_option_menu),
				  relation_menu);
	details->relation_menu = GTK_OPTION_MENU (relation_option_menu);
	gtk_widget_show_all (GTK_WIDGET(relation_option_menu));
	gtk_wrap_box_pack (GTK_WRAP_BOX (details->box),
			   GTK_WIDGET (details->relation_menu),
			   FALSE,
			   FALSE,
			   FALSE,
			   FALSE);

	g_assert (! (use_value_entry && use_value_menu));
	details->use_value_entry = use_value_entry;
	if (use_value_entry) {
		details->value_entry = NAUTILUS_ENTRY (nautilus_entry_new ());
		gtk_widget_show_all(GTK_WIDGET(details->value_entry));
		gtk_wrap_box_pack (GTK_WRAP_BOX (details->box),
				   GTK_WIDGET (details->value_entry),
				   FALSE,
				   FALSE,
				   FALSE,
				   FALSE);

		nautilus_undo_set_up_nautilus_entry_for_undo (details->value_entry);
		nautilus_undo_editable_set_undo_key (GTK_EDITABLE (details->value_entry), TRUE);
	}
	details->use_value_menu = use_value_menu;
	if (use_value_menu && details->type != NAUTILUS_EMBLEM_SEARCH_CRITERION) {
		g_return_val_if_fail (value_options != NULL, NULL);
		value_option_menu = gtk_option_menu_new ();
		value_menu = gtk_menu_new ();
		for (i = 0; value_options[i] != NULL; i++) {
			GtkWidget *item;
			context_stripped_value = eel_str_remove_bracketed_text (_(value_options[i]));
			item = gtk_menu_item_new_with_label (context_stripped_value);
			g_free (context_stripped_value);
			gtk_widget_show (item);
			gtk_object_set_data (GTK_OBJECT(item), "type", GINT_TO_POINTER(i));
			gtk_menu_append (GTK_MENU (value_menu),
					 item);
		}
		gtk_option_menu_set_menu (GTK_OPTION_MENU (value_option_menu),
					  value_menu);
		details->value_menu = GTK_OPTION_MENU (value_option_menu);
		gtk_widget_show_all (GTK_WIDGET(details->value_menu));
		gtk_wrap_box_pack (GTK_WRAP_BOX (details->box),
				   GTK_WIDGET (details->value_menu),
				   FALSE,
				   FALSE,
				   FALSE,
				   FALSE);
	}
	details->use_value_suffix = use_value_suffix;
	if (use_value_suffix) {
		g_return_val_if_fail (value_suffix != NULL, NULL);
		details->value_suffix = GTK_LABEL (gtk_label_new (value_suffix));
		gtk_widget_show_all (GTK_WIDGET (details->value_suffix));
		gtk_wrap_box_pack (GTK_WRAP_BOX (details->box),
				   GTK_WIDGET (details->value_suffix),
				   FALSE,
				   FALSE,
				   FALSE,
				   FALSE);
	}
	/* Special case widgets here */
	if (details->type == NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION) {
		details->date = GNOME_DATE_EDIT (gnome_date_edit_new (time (NULL), FALSE, FALSE));
		gtk_widget_show_all (GTK_WIDGET (details->date));
		gtk_wrap_box_pack (GTK_WRAP_BOX (details->box),
				   GTK_WIDGET (details->date),
				   FALSE,
				   FALSE,
				   FALSE,
				   FALSE);
	}
	if (details->type == NAUTILUS_EMBLEM_SEARCH_CRITERION) {
		value_option_menu = gtk_option_menu_new ();
		details->value_menu = GTK_OPTION_MENU (value_option_menu);
		make_emblem_value_menu (criterion);
		gtk_wrap_box_pack (GTK_WRAP_BOX (details->box),
				   GTK_WIDGET (details->value_menu),
				   FALSE,
				   FALSE,
				   FALSE,
				   FALSE);

	}

	return criterion;
}


NautilusSearchBarCriterion *
nautilus_search_bar_criterion_next_new (NautilusSearchBarCriterionType criterion_type,
					NautilusComplexSearchBar *bar)
{
	NautilusSearchBarCriterionType next_type;

	next_type = get_next_criterion_type (criterion_type,
					     nautilus_complex_search_bar_get_search_criteria (bar));

	return nautilus_search_bar_criterion_new_with_type (next_type, bar);

}

NautilusSearchBarCriterion *       
nautilus_search_bar_criterion_new_with_type (NautilusSearchBarCriterionType criterion_type,
					     NautilusComplexSearchBar *bar)
{
	NautilusSearchBarCriterion *new_criterion;

	switch(criterion_type) {
	case NAUTILUS_FILE_NAME_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_FILE_NAME_SEARCH_CRITERION,
									       name_relations,
									       TRUE,
									       FALSE,
									       NULL,
									       FALSE,
									       NULL,
									       bar);
 		break; 
	case NAUTILUS_CONTENT_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_CONTENT_SEARCH_CRITERION,
									       content_relations,
									       TRUE,
									       FALSE,
									       NULL,
									       FALSE,
									       NULL,
									       bar);
		break;
	case NAUTILUS_FILE_TYPE_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_FILE_TYPE_SEARCH_CRITERION,
									       type_relations,
									       FALSE,
									       TRUE,
									       type_objects,
									       FALSE,
									       NULL,
									       bar);
		break;
	case NAUTILUS_SIZE_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_SIZE_SEARCH_CRITERION,
									       size_relations,
									       TRUE,
									       FALSE,
									       NULL,
									       TRUE,
									       "K",
									       bar);
		break;
	case NAUTILUS_EMBLEM_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_EMBLEM_SEARCH_CRITERION,
									       emblem_relations,
									       FALSE,
									       FALSE,
									       NULL,
									       FALSE,
									       NULL,
									       bar);
									       
		break;
	case NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION,
									       modified_relations,
									       FALSE,
									       FALSE,
									       NULL,
									       FALSE,
									       NULL,
									       bar);
		break;
	case NAUTILUS_OWNER_SEARCH_CRITERION:
		new_criterion = nautilus_search_bar_criterion_new_from_values (NAUTILUS_OWNER_SEARCH_CRITERION,
									       owner_relations,
									       TRUE,
									       FALSE,
									       NULL,
									       FALSE,
									       NULL,
									       bar);
		break;
	default:
		new_criterion = NULL;
		g_assert_not_reached ();
	}
	if (new_criterion->details->value_entry) {
		nautilus_complex_search_bar_set_up_enclosed_entry_for_clipboard 
			(bar, new_criterion->details->value_entry);
	}
	return new_criterion;
}


NautilusSearchBarCriterion *
nautilus_search_bar_criterion_first_new (NautilusComplexSearchBar *bar)
{
	return nautilus_search_bar_criterion_new_from_values (NAUTILUS_FILE_NAME_SEARCH_CRITERION,
							      name_relations,
							      TRUE,
							      FALSE,
							      NULL,
							      FALSE,
							      NULL,
							      bar);
}

/* returns a newly allocated string: needs to be freed by the caller. */
char *
nautilus_search_bar_criterion_get_location (NautilusSearchBarCriterion *criterion)
{
	GtkWidget *menu;
	GtkWidget *menu_item;
	int name_number, relation_number, value_number;
	char *value_text;

	value_number = 0;
	value_text = NULL;
	/* There is ONE thing you should be aware of while implementing this function.
	   You have to make sure you use non-translated strings for building the uri.
	   So, to implement this, you are supposed to:
	   - build various tables which contain the strings corresponding to  a search type.
	   - call 
	   option_menu = gtk_option_menu_get_menu (criterion->details->some_menu)
	   menu_item = gtk_menu_get_active (option_menu)
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
	} else if (criterion->details->use_value_entry) {
		value_text = gtk_entry_get_text (GTK_ENTRY (criterion->details->value_entry));
	} else if (criterion->details->type == NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION) {
		value_text = eel_gnome_date_edit_get_date_as_string (criterion->details->date);
	}

	switch (name_number) {
	case NAUTILUS_FILE_NAME_SEARCH_CRITERION:
		g_assert (criterion->details->use_value_entry
			  || criterion->details->type == NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION);
		return get_name_location_for (relation_number,
					      value_text);
	case NAUTILUS_CONTENT_SEARCH_CRITERION:
		return get_content_location_for (relation_number,
						 value_text);
	case NAUTILUS_FILE_TYPE_SEARCH_CRITERION:
		g_assert (criterion->details->use_value_menu);
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
		break;
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
		return NAUTILUS_NUMBER_OF_SEARCH_CRITERIA;
	}
	return NAUTILUS_NUMBER_OF_SEARCH_CRITERIA;
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
	gtk_widget_show (criterion->details->box);
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
	gtk_widget_hide (criterion->details->box);
}

void                              
nautilus_search_bar_criterion_update_valid_criteria_choices (NautilusSearchBarCriterion *criterion,
							     GSList *current_criteria)
{
	GtkWidget *old_menu, *new_menu;
	GtkWidget *item;
	char *context_stripped_criteria_title;
	guint i;

	/* We remove the whole menu and put in a new one. */
	new_menu = gtk_menu_new ();
	for (i = 0; criteria_titles[i] != NULL; i++) {
		context_stripped_criteria_title = eel_str_remove_bracketed_text (_(criteria_titles[i]));
		item = gtk_menu_item_new_with_label (context_stripped_criteria_title);
		g_free (context_stripped_criteria_title);
		
		gtk_object_set_data (GTK_OBJECT(item), "type", GINT_TO_POINTER(i));
		
		gtk_signal_connect (GTK_OBJECT (item),
				    "activate",
				    criterion_type_changed_callback,
				    (gpointer) criterion);
		gtk_menu_append (GTK_MENU (new_menu),
				 item);
		gtk_widget_show (item);
		if (i == criterion->details->type) {
			gtk_menu_set_active (GTK_MENU (new_menu), i);
		}
		if (i != criterion->details->type && 
		    criterion_type_already_is_displayed (current_criteria, i)) {
			gtk_widget_set_sensitive (item, FALSE);
			continue;
		}
	}
	
	old_menu = gtk_option_menu_get_menu (criterion->details->available_criteria);

	gtk_object_destroy (GTK_OBJECT (old_menu));
	gtk_option_menu_set_menu (criterion->details->available_criteria,
				  new_menu);

	gtk_widget_show_all (GTK_WIDGET (criterion->details->available_criteria));
	
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

	matches = sscanf (unescaped_uri, "search:[file:///]%s", first_criterion);
	g_free (unescaped_uri);
	
	if (matches == 0) {
		g_free (first_criterion);
		return NULL;
	}

	return first_criterion;
}

static gboolean                     
modified_relation_should_show_value (int relation_index)
{
	return modified_relation_shows_date[relation_index];
}

static void
hide_date_widget (GtkObject *object,
		  gpointer data)
{
	NautilusSearchBarCriterion *criterion;
	NautilusSearchBarCriterionDetails *details;

	criterion = NAUTILUS_SEARCH_BAR_CRITERION (data);
	details = criterion->details;

	gtk_widget_hide (GTK_WIDGET (details->date));
	nautilus_complex_search_bar_queue_resize (details->bar);
}

static void
show_date_widget (GtkObject *object,
		  gpointer data)
{
	NautilusSearchBarCriterion *criterion;
	NautilusSearchBarCriterionDetails *details;

	criterion = NAUTILUS_SEARCH_BAR_CRITERION (data);
	details = criterion->details;

	gtk_widget_show (GTK_WIDGET (details->date));
	nautilus_complex_search_bar_queue_resize (details->bar);
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

	return g_strdup_printf ("%s %s %s",
				NAUTILUS_SEARCH_URI_TEXT_NAME,
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

	return g_strdup_printf ("%s %s %s",
				NAUTILUS_SEARCH_URI_TEXT_CONTENT, 
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

	return g_strdup_printf ("%s %s %s",
				NAUTILUS_SEARCH_URI_TEXT_TYPE, 
				possible_relations[relation_number],
				possible_values[value_number]);
}


static char *                              
get_size_location_for (int relation_number,
		       char *size_text)
{
	const char *possible_relations[] = { "larger_than", "smaller_than" };
	int entered_size;
	gboolean int_conversion_success;
	
	g_assert (relation_number == 0 || relation_number == 1);
	/* We put a 'K' after the size, so multiply what the user
	   entered by 1000 */
	int_conversion_success = eel_str_to_int (size_text, 
						      &entered_size);
	
	if (int_conversion_success) {
		return g_strdup_printf ("%s %s %d", NAUTILUS_SEARCH_URI_TEXT_SIZE, 
					possible_relations[relation_number], 
					entered_size * 1024);
	}
	else {
		return g_strdup_printf ("%s %s %s", NAUTILUS_SEARCH_URI_TEXT_SIZE, 
					possible_relations[relation_number], 
					size_text);
	}

}

static char *                              
get_emblem_location_for  (int relation_number,
			  GtkWidget *menu_item)
{
	const char *possible_relations[] = {"include", "do_not_include" };
	char *emblem_text;
	
	g_assert (relation_number == 0 ||
		  relation_number == 1);
	emblem_text = gtk_object_get_data (GTK_OBJECT (menu_item),
					   "emblem name");
	printf ("%s %s %s", NAUTILUS_SEARCH_URI_TEXT_EMBLEMS, possible_relations[relation_number], emblem_text);
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
					     "", 
					     "is_today",
					     "is_yesterday",
					     "",
					     "is_within_a_week_of", 
					     "is_within_a_month_of" };
	char *result;

	g_assert (relation_number >= 0);
	g_assert (relation_number < 10);
	g_return_val_if_fail (relation_number != 4 && relation_number != 7, g_strdup (""));
	
	/* Handle "is today" and "is yesterday" separately */
	if (relation_number == 5) {
		result = g_strdup_printf ("%s is today", NAUTILUS_SEARCH_URI_TEXT_DATE_MODIFIED);
	} else if (relation_number == 6) {
		result = g_strdup_printf ("%s is yesterday", NAUTILUS_SEARCH_URI_TEXT_DATE_MODIFIED);
	} else  if (date_string != NULL) {
		result = g_strdup_printf ("%s %s %s", NAUTILUS_SEARCH_URI_TEXT_DATE_MODIFIED,
					  possible_relations[relation_number],
					  date_string);
	} else {
		result = g_strdup ("");
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
	const char *possible_relations[] = { "is", "is_not" };
	g_assert (relation_number == 0 || relation_number == 1);
	return g_strdup_printf ("%s %s %s",
				NAUTILUS_SEARCH_URI_TEXT_OWNER,
				possible_relations[relation_number],
				owner_text);
}

static void                                
make_emblem_value_menu (NautilusSearchBarCriterion *criterion)
{
	NautilusCustomizationData *customization_data;
	GtkWidget *menu_item;
	char *emblem_name, *dot_pos;
	GtkWidget *value_menu; 
	GdkPixbuf *pixbuf;
	char *label;
	
	/* Add the items to the emblems menu here */
	value_menu = gtk_menu_new ();
	customization_data = nautilus_customization_data_new ("emblems",
							      TRUE,
							      TRUE,
							      NAUTILUS_ICON_SIZE_FOR_MENUS, 
							      NAUTILUS_ICON_SIZE_FOR_MENUS);
	while (nautilus_customization_data_get_next_element_for_display (customization_data,
									 &emblem_name,
									 &pixbuf,
									 &label) == GNOME_VFS_OK) {
		GtkWidget *image;

		/* remove the suffix, if any, to make the emblem name */
		dot_pos = strrchr (emblem_name, '.');
		if (dot_pos) {
			*dot_pos = '\0';
		}
		
		if (strcmp (emblem_name, "erase") == 0) {
			gdk_pixbuf_unref (pixbuf);
			g_free (label);
			g_free (emblem_name);
			continue;
		}
		menu_item = gtk_menu_item_new ();
		
		gtk_object_set_data_full (GTK_OBJECT (menu_item), "emblem name",
					  g_strdup (emblem_name), (GtkDestroyNotify) g_free);

		
		image = eel_labeled_image_new (label, pixbuf);
		eel_labeled_image_set_label_position (EEL_LABELED_IMAGE (image), GTK_POS_RIGHT);
		eel_labeled_image_set_x_alignment (EEL_LABELED_IMAGE (image), 0.0);
		eel_labeled_image_set_spacing (EEL_LABELED_IMAGE (image), 4);

		gtk_container_add (GTK_CONTAINER (menu_item), image);
		gtk_widget_show_all (menu_item);
		gtk_menu_append (GTK_MENU (value_menu), menu_item);

		gdk_pixbuf_unref (pixbuf);
		g_free (label);
		g_free (emblem_name);
	}
	
	gtk_widget_show_all (GTK_WIDGET (criterion->details->value_menu));		
	gtk_option_menu_set_menu (criterion->details->value_menu,
				  value_menu);
	criterion->details->use_value_menu = TRUE;
}



static void
criterion_type_changed_callback (GtkObject *object,
				 gpointer data)
{
	NautilusSearchBarCriterion *criterion;
	GtkWidget *menu_item;

	g_return_if_fail (NAUTILUS_IS_SEARCH_BAR_CRITERION (data));
	g_return_if_fail (GTK_IS_MENU_ITEM (object));
	criterion = NAUTILUS_SEARCH_BAR_CRITERION (data);
	menu_item = GTK_WIDGET (object);

	g_return_if_fail (NAUTILUS_IS_COMPLEX_SEARCH_BAR (criterion->details->bar));
	gtk_object_set_data (GTK_OBJECT (criterion), "type", 
			     gtk_object_get_data (GTK_OBJECT (menu_item), "type"));
	gtk_signal_emit (GTK_OBJECT (criterion),
			 signals[CRITERION_TYPE_CHANGED]);
	
}

static void
emblems_changed_callback (GtkObject *signaller,
			  gpointer data)
{
	NautilusSearchBarCriterion *criterion;
	GtkWidget *menu_widget;

	g_return_if_fail (NAUTILUS_IS_SEARCH_BAR_CRITERION (data));
	criterion = NAUTILUS_SEARCH_BAR_CRITERION (data);

	if (criterion->details->type == NAUTILUS_EMBLEM_SEARCH_CRITERION) {
		/* Get rid of the old menu */
		menu_widget = gtk_option_menu_get_menu (criterion->details->value_menu);
		gtk_option_menu_remove_menu (criterion->details->value_menu);
		gtk_widget_destroy (menu_widget);
		make_emblem_value_menu (criterion);
	}
}

static gint
criterion_is_of_type (gconstpointer a,
		      gconstpointer b) 
{
	NautilusSearchBarCriterion *criterion;
	NautilusSearchBarCriterionType type;

	g_return_val_if_fail (NAUTILUS_IS_SEARCH_BAR_CRITERION (a), 
			      NAUTILUS_NUMBER_OF_SEARCH_CRITERIA);
	criterion = NAUTILUS_SEARCH_BAR_CRITERION (a);
	type = (NautilusSearchBarCriterionType) b;
	
	return (criterion->details->type - type);
}

static gboolean                            
criterion_type_already_is_displayed (GSList *criteria,
				     NautilusSearchBarCriterionType criterion_number)
{
	if (g_slist_find_custom (criteria,
				 GINT_TO_POINTER (criterion_number),
				 criterion_is_of_type)) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}



static NautilusSearchBarCriterionType
get_next_criterion_type (NautilusSearchBarCriterionType current_type,
			 GSList *displayed_criteria)
{
	NautilusSearchBarCriterionType new_type;

	g_assert (g_slist_length (displayed_criteria) < NAUTILUS_NUMBER_OF_SEARCH_CRITERIA);
	
	new_type = (current_type + 1) % NAUTILUS_NUMBER_OF_SEARCH_CRITERIA;
	while (criterion_type_already_is_displayed (displayed_criteria,
						    new_type)) {
		new_type = (new_type + 1) % NAUTILUS_NUMBER_OF_SEARCH_CRITERIA;
	}
	
	return new_type;
}

