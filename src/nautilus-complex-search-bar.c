/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Authors: Rebecca Schulman <rebecka@eazel.com>
 */

/* nautilus-complex-search-bar.c - Search bar containing many attributes
 */

#include <config.h>
#include "nautilus-complex-search-bar.h"

#include "nautilus-search-bar-criterion-private.h"
#include "nautilus-search-bar-criterion.h"
#include "nautilus-window-private.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dock.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus/nautilus-clipboard.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-string.h>
#include <libnautilus-private/nautilus-undo-signal-handlers.h>
#include "gtkhwrapbox.h"

struct NautilusComplexSearchBarDetails {
	GtkVBox *bar_container;
	GtkWidget *criteria_container;

	GtkWidget *more_options;
	GtkWidget *fewer_options;
	GtkWidget *find_them;
	
	/* For use in setting up clipboard */
	NautilusWindow *window;

	GSList *search_criteria;
};

static void  real_activate				  (NautilusNavigationBar	       *bar);
static char *nautilus_complex_search_bar_get_location     (NautilusNavigationBar         *bar);
static void  nautilus_complex_search_bar_set_location     (NautilusNavigationBar         *bar,
							   const char                    *location);
static void  nautilus_complex_search_bar_initialize_class (NautilusComplexSearchBarClass *class);
static void  nautilus_complex_search_bar_initialize       (NautilusComplexSearchBar      *bar);
static void  nautilus_complex_search_bar_destroy 	  (GtkObject 			 *object);
static void  attach_criterion_to_search_bar               (NautilusComplexSearchBar      *bar,
							   NautilusSearchBarCriterion    *criterion,
							   int                           position);

static void  unattach_criterion_from_search_bar           (NautilusComplexSearchBar      *bar,
							   NautilusSearchBarCriterion    *criterion);
static void  more_options_callback                        (GtkObject                     *object,
							   gpointer                       data);
static void  fewer_options_callback                       (GtkObject                     *object,
							   gpointer                       data);
static void  search_bar_criterion_type_changed_callback   (GtkObject *old_criterion_object,
							   gpointer data);
static GtkWidget * load_find_them_pixmap_widget           (void);

static void	   update_options_buttons_state 	  (NautilusComplexSearchBar *bar);
static void	   update_find_button_state 	  	  (NautilusComplexSearchBar *bar);
static void	   update_dynamic_buttons_state 	  (NautilusComplexSearchBar *bar);
static void        update_criteria_choices                (gpointer list_item,
							   gpointer data);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusComplexSearchBar, nautilus_complex_search_bar, NAUTILUS_TYPE_SEARCH_BAR)

/* called by the criterion when the user chooses
   a new criterion type */
static void 
search_bar_criterion_type_changed_callback (GtkObject *old_criterion_object,
					    gpointer data)
{ 
	NautilusSearchBarCriterionType new_type;
	NautilusSearchBarCriterion *criterion, *new_criterion;
	GSList *old_criterion_location;
	NautilusComplexSearchBar *bar;

	g_return_if_fail (NAUTILUS_IS_SEARCH_BAR_CRITERION (old_criterion_object));
	g_return_if_fail (NAUTILUS_IS_COMPLEX_SEARCH_BAR (data));

	criterion = NAUTILUS_SEARCH_BAR_CRITERION (old_criterion_object);
	bar = NAUTILUS_COMPLEX_SEARCH_BAR (data);

	/* First create the new criterion with the type that was activated */
	new_type = GPOINTER_TO_INT (gtk_object_get_data (old_criterion_object,
							 "type"));
	new_criterion = nautilus_search_bar_criterion_new_with_type (new_type, 
								     bar);
	gtk_signal_connect (GTK_OBJECT (new_criterion),
			    "criterion_type_changed",
			    search_bar_criterion_type_changed_callback,
			    (gpointer) bar);
	old_criterion_location = g_slist_find (bar->details->search_criteria,
					       criterion);
	old_criterion_location->data = new_criterion;
	unattach_criterion_from_search_bar (bar, criterion);
	gtk_object_sink (GTK_OBJECT (criterion));
	nautilus_search_bar_criterion_show (new_criterion);
	attach_criterion_to_search_bar (bar,
					new_criterion,
					g_slist_position (bar->details->search_criteria,
							  old_criterion_location));
	
	/* Then tell the other criteria to take update their
	   menus to reflect the other criteria you can really choose now */
	g_slist_foreach (bar->details->search_criteria,
			 update_criteria_choices,
			 bar);

	update_dynamic_buttons_state (bar);
}


static void
update_criteria_choices (gpointer list_item,
			 gpointer data)
{
	NautilusSearchBarCriterion *criterion;
	NautilusComplexSearchBar *bar;

	g_return_if_fail (NAUTILUS_IS_SEARCH_BAR_CRITERION (list_item));
	g_return_if_fail (NAUTILUS_IS_COMPLEX_SEARCH_BAR (data));
	criterion = NAUTILUS_SEARCH_BAR_CRITERION (list_item);
	bar = NAUTILUS_COMPLEX_SEARCH_BAR (data);

	nautilus_search_bar_criterion_update_valid_criteria_choices (criterion,
								     bar->details->search_criteria);
	
}

static void
queue_search_bar_resize_callback (GtkObject *search_bar,
				  gpointer data)
{
	NautilusComplexSearchBar *bar;

	bar = NAUTILUS_COMPLEX_SEARCH_BAR (data);
	if (GTK_WIDGET_VISIBLE (search_bar)) {
		nautilus_complex_search_bar_queue_resize (bar);
	}
}
     
static void
nautilus_complex_search_bar_initialize_class (NautilusComplexSearchBarClass *klass)
{
	GtkObjectClass *object_class;
	NautilusNavigationBarClass *navigation_bar_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = nautilus_complex_search_bar_destroy;

	navigation_bar_class = NAUTILUS_NAVIGATION_BAR_CLASS (klass);
	navigation_bar_class->activate = real_activate;
	navigation_bar_class->get_location = nautilus_complex_search_bar_get_location;
	navigation_bar_class->set_location = nautilus_complex_search_bar_set_location;

}

static void
nautilus_complex_search_bar_initialize (NautilusComplexSearchBar *bar)
{
	NautilusSearchBarCriterion *file_name_criterion;
	GtkWidget *hbox;
	GtkWidget *find_them_box, *find_them_pixmap_widget, *find_them_label;
	
	bar->details = g_new0 (NautilusComplexSearchBarDetails, 1);
	
	bar->details->bar_container = GTK_VBOX (gtk_vbox_new (FALSE, 1));

	bar->details->criteria_container = gtk_vbox_new (FALSE, 1);

	gtk_container_set_resize_mode (GTK_CONTAINER (bar->details->criteria_container),
				       GTK_RESIZE_IMMEDIATE);

	/* Create button before criterion so we text fields can hook to criterion's signal */
	bar->details->find_them = gtk_button_new ();

	file_name_criterion = nautilus_search_bar_criterion_first_new (bar);

	gtk_signal_connect (GTK_OBJECT (file_name_criterion),
			    "criterion_type_changed",
			    search_bar_criterion_type_changed_callback,
			    (gpointer) bar);
	bar->details->search_criteria = g_slist_prepend (NULL,
							 file_name_criterion);

	nautilus_search_bar_criterion_show (file_name_criterion);
	attach_criterion_to_search_bar (bar, file_name_criterion, 1);

	gtk_box_pack_start (GTK_BOX (bar->details->bar_container),
			    GTK_WIDGET (bar->details->criteria_container),
			    TRUE,
			    FALSE,
			    0);
	gtk_widget_show (GTK_WIDGET (bar->details->criteria_container));

	hbox = gtk_hwrap_box_new (FALSE);

	gtk_signal_connect (GTK_OBJECT (hbox),
			    "need_reallocation",
			    queue_search_bar_resize_callback,
			    bar);

	bar->details->more_options = gtk_button_new_with_label (_("More Options"));
	gtk_signal_connect (GTK_OBJECT (bar->details->more_options), "clicked",
			    more_options_callback, bar);
				
				
	gtk_wrap_box_pack (GTK_WRAP_BOX (hbox),
			   bar->details->more_options,
			   FALSE, FALSE, FALSE, FALSE);
	gtk_widget_show (bar->details->more_options);

	bar->details->fewer_options = gtk_button_new_with_label (_("Fewer Options"));
	gtk_signal_connect (GTK_OBJECT (bar->details->fewer_options), "clicked",
			    fewer_options_callback, bar);

	gtk_wrap_box_pack (GTK_WRAP_BOX (hbox),
			    bar->details->fewer_options,
			   FALSE, FALSE, FALSE, FALSE);

	gtk_widget_show (bar->details->fewer_options);

	find_them_box = gtk_hbox_new (FALSE, 1);
	find_them_pixmap_widget = load_find_them_pixmap_widget ();
	if (find_them_pixmap_widget != NULL)
		gtk_box_pack_start (GTK_BOX (find_them_box),
				    find_them_pixmap_widget,
				    TRUE,
				    FALSE,
				    1);
	find_them_label = gtk_label_new (_("Find Them!"));
	gtk_box_pack_start (GTK_BOX (find_them_box),
			    find_them_label,
			    TRUE,
			    FALSE,
			    1);

	gtk_container_add (GTK_CONTAINER (bar->details->find_them), find_them_box);
	gtk_signal_connect_object (GTK_OBJECT (bar->details->find_them), "clicked",
				   nautilus_navigation_bar_location_changed,
				   GTK_OBJECT (bar));

	gtk_wrap_box_pack (GTK_WRAP_BOX (hbox), 
			   bar->details->find_them, 
			   FALSE, FALSE,
			   FALSE, FALSE);
	gtk_widget_show (bar->details->find_them);
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
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static GtkWidget *
get_first_text_field (NautilusComplexSearchBar *bar)
{
	GSList *node;
	NautilusSearchBarCriterion *criterion;

	for (node = bar->details->search_criteria; node != NULL; node = node->next) {
		criterion = NAUTILUS_SEARCH_BAR_CRITERION (node->data);
		if (criterion->details->use_value_entry) {
			return GTK_WIDGET (criterion->details->value_entry);
		}
	}

	return NULL;
}

static void
real_activate (NautilusNavigationBar *navigation_bar)
{
	NautilusComplexSearchBar *bar;
	GtkWidget *initial_focus_widget;

	bar = NAUTILUS_COMPLEX_SEARCH_BAR (navigation_bar);

	/* Put the keyboard focus in a text field when switching to search mode */
	initial_focus_widget = get_first_text_field (bar);
	if (initial_focus_widget != NULL) {
		gtk_widget_grab_focus (initial_focus_widget);
	}
	nautilus_complex_search_bar_queue_resize (bar);
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

	trimmed_fragment = eel_str_strip_trailing_str (criteria_text, " & ");
	g_free (criteria_text);

	escaped_fragment = gnome_vfs_escape_string (trimmed_fragment);
	g_free (trimmed_fragment);

	search_uri = g_strconcat ("search:index-if-available", escaped_fragment, NULL);

	g_free (escaped_fragment);

	return search_uri;
}

static void                       
nautilus_complex_search_bar_set_location (NautilusNavigationBar *navigation_bar,
					  const char *location)
{
	NautilusComplexSearchBar *bar;

	bar = NAUTILUS_COMPLEX_SEARCH_BAR (navigation_bar);

	/* FIXME bugzilla.gnome.org 42517: Not implemented. */
}

void  
nautilus_complex_search_bar_queue_resize (NautilusComplexSearchBar      *bar)
{
	GtkWidget *dock;

	gtk_widget_queue_resize (bar->details->criteria_container);
	/* FIXME bugzilla.gnome.org 43171:
	 * (It is possible this comment is no longer correct due to
	 * a change in the layout code)
	 * We don't know why this line is needed here, but if it's removed
	 * then the bar sometimes won't shrink when we press the fewer options
	 * button. Specifically, if the window is very wide, then it won't 
	 * shrink when pressing the fewer options button, but it will if the
	 * window is fairly narrow.
	 */
	dock = gtk_widget_get_ancestor (GTK_WIDGET (bar), GNOME_TYPE_DOCK);
	if (dock != NULL) {
		gtk_widget_queue_resize (dock);
	}
	

}

static void
attach_criterion_to_search_bar (NautilusComplexSearchBar *bar,
				NautilusSearchBarCriterion *criterion,
				int position)
{
	g_return_if_fail (NAUTILUS_IS_COMPLEX_SEARCH_BAR (bar));

	gtk_box_pack_start (GTK_BOX (bar->details->criteria_container),
			    GTK_WIDGET (criterion->details->box),			  
			    FALSE,
			    FALSE,
			    1);
	gtk_box_reorder_child (GTK_BOX (bar->details->criteria_container),
			       GTK_WIDGET (criterion->details->box),
			       position);
			       
	g_assert (criterion->details->use_value_entry + 
		  criterion->details->use_value_menu == 1 ||
		  criterion->details->type == NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION);
	
	if (criterion->details->use_value_entry) {
		/* We want to track whether the entry text is empty or not. */
		gtk_signal_connect_object (GTK_OBJECT (criterion->details->value_entry),
					   "changed", 
					   update_find_button_state, 
					   GTK_OBJECT (bar));
		
		/* We want to activate the "Find" button when any entry text is not empty */
		g_assert (GTK_IS_BUTTON (bar->details->find_them));
		gtk_signal_connect_object (GTK_OBJECT (criterion->details->value_entry), 
					   "activate",
					   eel_gtk_button_auto_click, 
					   GTK_OBJECT (bar->details->find_them));
	}
	nautilus_complex_search_bar_queue_resize (bar);
}

static void
unattach_criterion_from_search_bar (NautilusComplexSearchBar *bar,
				    NautilusSearchBarCriterion *criterion)
{

	gtk_container_remove (GTK_CONTAINER (bar->details->criteria_container),
			      GTK_WIDGET (criterion->details->box));

	g_assert ((criterion->details->use_value_entry + 
		   criterion->details->use_value_menu == 1) ||
		  criterion->details->type == NAUTILUS_DATE_MODIFIED_SEARCH_CRITERION);
	nautilus_complex_search_bar_queue_resize (bar);
}

static GtkWidget *
load_find_them_pixmap_widget (void)
{
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GtkWidget *widget;
	
	pixbuf = gdk_pixbuf_new_from_file (NAUTILUS_PIXMAPDIR "/search.png");
	if (pixbuf != NULL) {
		gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, EEL_STANDARD_ALPHA_THRESHHOLD);
		gdk_pixbuf_unref (pixbuf);
		widget = gtk_pixmap_new (pixmap, mask);

		gdk_pixmap_unref (pixmap);
		gdk_pixmap_unref (mask);
 
		return widget;
	} else {
		return NULL;
	}
}
				  

GtkWidget *
nautilus_complex_search_bar_new (NautilusWindow *window)
{
	GtkWidget *bar;
	NautilusSearchBarCriterion *first_criterion;

	g_return_val_if_fail (NAUTILUS_IS_WINDOW (window), NULL);

	bar = gtk_widget_new (NAUTILUS_TYPE_COMPLEX_SEARCH_BAR, NULL);
	gtk_object_set_data (GTK_OBJECT (bar), "associated_window", window);
	
	/* Set up the first criterion's entry for the clipboard */
	first_criterion = NAUTILUS_COMPLEX_SEARCH_BAR (bar)->details->search_criteria->data;
	g_assert (first_criterion != NULL);
	g_assert (first_criterion->details->use_value_entry);

	nautilus_clipboard_set_up_editable 
		(GTK_EDITABLE (first_criterion->details->value_entry),
		 nautilus_window_get_ui_container (window),
		 TRUE);

	return bar;
}

void
nautilus_complex_search_bar_set_up_enclosed_entry_for_clipboard (NautilusComplexSearchBar *bar,
								 NautilusEntry *entry)
{
	NautilusWindow *associated_window;

	associated_window = gtk_object_get_data (GTK_OBJECT (bar),
						 "associated_window");

	g_assert (associated_window != NULL);
	nautilus_clipboard_set_up_editable (GTK_EDITABLE (entry),
					    nautilus_window_get_ui_container (associated_window),
					    TRUE);
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
	criterion = nautilus_search_bar_criterion_next_new (last_criterion->details->type,
							    bar);
	gtk_signal_connect (GTK_OBJECT (criterion),
			    "criterion_type_changed",
			    search_bar_criterion_type_changed_callback,
			    (gpointer) bar);
	bar->details->search_criteria = g_slist_append (list, criterion);

	nautilus_search_bar_criterion_show (criterion);
	attach_criterion_to_search_bar (bar, criterion,
					g_slist_length (bar->details->search_criteria));

	update_dynamic_buttons_state (bar);
	g_slist_foreach (bar->details->search_criteria,
			 update_criteria_choices,
			 bar);	
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

	update_dynamic_buttons_state (bar);
	g_slist_foreach (bar->details->search_criteria,
			 update_criteria_choices,
			 bar);

}

static void
update_options_buttons_state (NautilusComplexSearchBar *bar)
{
	/* "Fewer Options" is enabled unless there's only one criterion */
	gtk_widget_set_sensitive (GTK_WIDGET (bar->details->fewer_options), g_slist_length (bar->details->search_criteria) > 1);
	gtk_widget_set_sensitive (GTK_WIDGET (bar->details->more_options), g_slist_length (bar->details->search_criteria) < NAUTILUS_NUMBER_OF_SEARCH_CRITERIA);

}

static gboolean
criteria_invalid (NautilusComplexSearchBar *bar)
{
	GSList *node;
	NautilusSearchBarCriterion *criterion;
	char *text;
	gboolean text_is_empty;
	int size;
	
	g_assert (NAUTILUS_IS_COMPLEX_SEARCH_BAR (bar));

	/* Walk through all value fields, checking whether any of them are empty. */
	/* Also check the value of the size entry, if it's open to make sure
	   it's actually a valid number */
	for (node = bar->details->search_criteria; node != NULL; node = node->next) {
		criterion = NAUTILUS_SEARCH_BAR_CRITERION (node->data);
		if (criterion->details->use_value_entry) {
			text = gtk_editable_get_chars 
				(GTK_EDITABLE (criterion->details->value_entry),
				0, -1);
			text_is_empty = eel_str_is_empty (text);
			if (criterion->details->type == NAUTILUS_SIZE_SEARCH_CRITERION) {
				if (!eel_str_to_int (text, &size)) {
					g_free (text);
					return TRUE;
				}
			}

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


GSList *   
nautilus_complex_search_bar_get_search_criteria (NautilusComplexSearchBar *bar)
{
	return bar->details->search_criteria;
}
