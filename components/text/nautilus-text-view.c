/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Andy Hertzfeld <andy@eazel.com>
 *
 */

/* text view - display a text file */

#include <config.h>
#include "nautilus-text-view.h"

#include <bonobo/bonobo-zoomable.h>
#include <bonobo/bonobo-control.h>

#include <libnautilus/libnautilus.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-bonobo-extensions.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-font-factory.h>

#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnorba/gnorba.h>
#include <limits.h>

#include <ghttp.h>

struct _NautilusTextViewDetails {
	NautilusFile *file;
	NautilusView *nautilus_view;
	BonoboZoomable *zoomable;
	int zoom_index;
	
	GtkWidget *container;
	GtkWidget *text_display;
	
	char *font_name;
	GdkFont *current_font;
};

static void nautilus_text_view_initialize_class			(NautilusTextViewClass *klass);
static void nautilus_text_view_initialize			(NautilusTextView      *view);
static void nautilus_text_view_destroy				(GtkObject              *object);
static void nautilus_text_view_update				(NautilusTextView      *text_view);
static void text_view_load_location_callback			(NautilusView           *view,
								 const char             *location,
								 NautilusTextView      *text_view);

static void  merge_bonobo_menu_items				(BonoboControl *control,
								 gboolean state,
								 gpointer user_data);

static void nautilus_text_view_update_font			(NautilusTextView *text_view);

static void zoomable_set_zoom_level_callback			(BonoboZoomable       *zoomable,
								 float                 level,
								 NautilusTextView      *view);
static void zoomable_zoom_in_callback				(BonoboZoomable       *zoomable,
								 NautilusTextView      *directory_view);
static void zoomable_zoom_out_callback				(BonoboZoomable       *zoomable,
								 NautilusTextView      *directory_view);
static void zoomable_zoom_to_fit_callback			(BonoboZoomable       *zoomable,
								 NautilusTextView      *directory_view);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusTextView,
                                   nautilus_text_view,
                                   GTK_TYPE_EVENT_BOX)

static float text_view_preferred_zoom_levels[] = { .25, .50, .75, 1.0, 1.5, 2.0, 4.0 };
static int   text_view_preferred_font_sizes[] = { 9, 10, 12, 14, 18, 24, 36 };

static const gint max_preferred_zoom_levels = (sizeof (text_view_preferred_zoom_levels) /
					       sizeof (float)) - 1;

static void
nautilus_text_view_initialize_class (NautilusTextViewClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	
	object_class->destroy = nautilus_text_view_destroy;
}

/* initialize ourselves by connecting to the location change signal and allocating our subviews */

static void
nautilus_text_view_initialize (NautilusTextView *text_view)
{
	GtkWidget *scrolled_window;
	
	text_view->details = g_new0 (NautilusTextViewDetails, 1);

	text_view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (text_view));

	/* set up the zoomable interface */
	text_view->details->zoomable = bonobo_zoomable_new ();
	text_view->details->zoom_index = 3;
	
	gtk_signal_connect (GTK_OBJECT (text_view->details->zoomable), "set_zoom_level",
			    GTK_SIGNAL_FUNC (zoomable_set_zoom_level_callback), text_view);
	gtk_signal_connect (GTK_OBJECT (text_view->details->zoomable), "zoom_in",
			    GTK_SIGNAL_FUNC (zoomable_zoom_in_callback), text_view);
	gtk_signal_connect (GTK_OBJECT (text_view->details->zoomable), "zoom_out",
			    GTK_SIGNAL_FUNC (zoomable_zoom_out_callback), text_view);
	gtk_signal_connect (GTK_OBJECT (text_view->details->zoomable), "zoom_to_fit",
			    GTK_SIGNAL_FUNC (zoomable_zoom_to_fit_callback), text_view);
	
	bonobo_zoomable_set_parameters_full (text_view->details->zoomable,
					     1.0, .25, 4.0, TRUE, TRUE, FALSE,
					     text_view_preferred_zoom_levels, NULL,
					     NAUTILUS_N_ELEMENTS (text_view_preferred_zoom_levels));
	
	bonobo_object_add_interface (BONOBO_OBJECT (text_view->details->nautilus_view),
				     BONOBO_OBJECT (text_view->details->zoomable));
 
    	
	gtk_signal_connect (GTK_OBJECT (text_view->details->nautilus_view), 
			    "load_location",
			    text_view_load_location_callback, 
			    text_view);
			    	
	/* set up the default font */
	text_view->details->font_name = g_strdup ("helvetica"); /* eventually, get this from preferences */	

	/* allocate a vbox to contain the text widget */
	
	text_view->details->container = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (text_view->details->container), 0);
	gtk_container_add (GTK_CONTAINER (text_view), GTK_WIDGET (text_view->details->container));
	
	gtk_widget_show (GTK_WIDGET (text_view->details->container));

	/* allocate the text object */
	text_view->details->text_display = gtk_text_new (NULL, NULL);
	gtk_widget_show (text_view->details->text_display);
	gtk_text_set_editable (GTK_TEXT (text_view->details->text_display), TRUE);

	/* set the font of the text object */
	nautilus_text_view_update_font (text_view);
	
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scrolled_window), text_view->details->text_display);
	gtk_widget_show (scrolled_window);
	gtk_container_add (GTK_CONTAINER (text_view->details->container), scrolled_window);

	/* get notified when we are activated so we can merge in our menu items */
        gtk_signal_connect (GTK_OBJECT (nautilus_view_get_bonobo_control
					(text_view->details->nautilus_view)),
                            "activate",
                            merge_bonobo_menu_items,
                            text_view);
		 	
	/* finally, show the view itself */	
	gtk_widget_show (GTK_WIDGET (text_view));
}

static void
detach_file (NautilusTextView *text_view)
{
        if (text_view->details->file != NULL) {
                nautilus_file_unref (text_view->details->file);
                text_view->details->file = NULL;
        }
}

static void
nautilus_text_view_destroy (GtkObject *object)
{
	NautilusTextView *text_view;
	
        text_view = NAUTILUS_TEXT_VIEW (object);

        detach_file (text_view);
	gdk_font_unref (text_view->details->current_font);
	 
	g_free (text_view->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}



/* Component embedding support */
NautilusView *
nautilus_text_view_get_nautilus_view (NautilusTextView *text_view)
{
	return text_view->details->nautilus_view;
}



/* here's where we do most of the real work of populating the view with info from the new uri */
static void
nautilus_text_view_update (NautilusTextView *text_view) 
{
        char *uri;
	int text_size;
        int position;
	char *text_ptr;
	
        uri = nautilus_file_get_uri (text_view->details->file);
        gtk_editable_delete_text (GTK_EDITABLE (text_view->details->text_display), 0, -1);   

	if (nautilus_read_entire_file (uri, &text_size, &text_ptr) == GNOME_VFS_OK) {
        	position = 0; 

		gtk_text_insert (GTK_TEXT (text_view->details->text_display),
			 NULL, NULL, NULL,
			 text_ptr, text_size);
        			
		g_free (text_ptr);
	}
				
        g_free (uri);
}


void
nautilus_text_view_load_uri (NautilusTextView *text_view, const char *uri)
{
 
        detach_file (text_view);
        text_view->details->file = nautilus_file_get (uri);
	
	nautilus_text_view_update (text_view);
}

static void
text_view_load_location_callback (NautilusView *view, 
                                   const char *location,
                                   NautilusTextView *text_view)
{
        nautilus_view_report_load_underway (text_view->details->nautilus_view);
	nautilus_text_view_load_uri (text_view, location);
        nautilus_view_report_load_complete (text_view->details->nautilus_view);
}

/* update the font and redraw */
static void
nautilus_text_view_update_font (NautilusTextView *text_view)
{
	int point_size;
	point_size = text_view_preferred_font_sizes[text_view->details->zoom_index];

	if (text_view->details->current_font != NULL) {
		gdk_font_unref (text_view->details->current_font);
	}
	
	text_view->details->current_font =  nautilus_font_factory_get_font_by_family (text_view->details->font_name, point_size);
	nautilus_gtk_widget_set_font (text_view->details->text_display, text_view->details->current_font);

	gtk_editable_changed (GTK_EDITABLE (text_view->details->text_display));
}

/* handle merging in the menu items */

/* here are the callbacks to handle bonobo menu items.  Initially, this is ad hoc for prototyping */
/* but soon we'll make a generalized framework */

static char *
get_selected_text (GtkEditable *text_widget)
{
	if (!text_widget->has_selection || text_widget->selection_start_pos == text_widget->selection_end_pos) {
		return NULL;
	}

	return gtk_editable_get_chars (text_widget, text_widget->selection_start_pos, text_widget->selection_end_pos);
}

static void
text_view_search_callback (BonoboUIComponent *ui, gpointer user_data, const char *verb)
{
	NautilusTextView *text_view;
	char *selected_text, *mapped_text, *uri;
	
	text_view = NAUTILUS_TEXT_VIEW (user_data);
	
	/* get the selection */
	selected_text = get_selected_text (GTK_EDITABLE (text_view->details->text_display));
	
	if (selected_text) {
		/* formulate the url */
		mapped_text = gnome_vfs_escape_string (selected_text);
		uri = g_strdup_printf ("http://www.google.com/search?q=%s", mapped_text);
			
		/* goto the url */	
		nautilus_view_open_location (text_view->details->nautilus_view, uri);

		g_free (uri);
		g_free (selected_text);
		g_free (mapped_text);
	}
}

static void
text_view_lookup_callback (BonoboUIComponent *ui, gpointer user_data, const char *verb)
{
	NautilusTextView *text_view;
	char *selected_text, *mapped_text, *uri;
	
	text_view = NAUTILUS_TEXT_VIEW (user_data);
	
	/* get the selection */
	selected_text = get_selected_text (GTK_EDITABLE (text_view->details->text_display));
	
	if (selected_text) {
		/* formulate the url */
		mapped_text = gnome_vfs_escape_string (selected_text);
		uri = g_strdup_printf ("http://www.m-w.com/cgi-bin/dictionary?va=%s", mapped_text);
		
		/* goto the url */	
		nautilus_view_open_location (text_view->details->nautilus_view, uri);

		g_free (uri);
		g_free (selected_text);
		g_free (mapped_text);
	}

}

/* handle the font menu items */

static void
nautilus_text_view_set_font (NautilusTextView *text_view, const char *font_family)
{
	if (nautilus_strcmp (text_view->details->font_name, font_family) == 0) {
		return;
	}

	g_free (text_view->details->font_name);
	text_view->details->font_name = g_strdup (font_family);
	
	nautilus_text_view_update_font (text_view);				
}

static void
handle_ui_event (BonoboUIComponent *ui,
		 const char *id,
		 Bonobo_UIComponent_EventType type,
		 const char *state,
		 NautilusTextView *view)
{
	if (type == Bonobo_UIComponent_STATE_CHANGED
	    && strcmp (state, "1") == 0) {
		nautilus_text_view_set_font (NAUTILUS_TEXT_VIEW (view), id);
	}
}

/* this routine is invoked when the view is activated to merge in our menu items */
static void
merge_bonobo_menu_items (BonoboControl *control, gboolean state, gpointer user_data)
{
 	NautilusTextView *text_view;
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("Search", text_view_search_callback),
		BONOBO_UI_VERB ("Lookup", text_view_lookup_callback),

		BONOBO_UI_VERB_END
	};

	g_assert (BONOBO_IS_CONTROL (control));
	
	text_view = NAUTILUS_TEXT_VIEW (user_data);

	if (state) {
		nautilus_view_set_up_ui (text_view->details->nautilus_view,
				         DATADIR,
					 "nautilus-text-view-ui.xml",
					 "nautilus-text-view");
									
		bonobo_ui_component_add_verb_list_with_data 
			(bonobo_control_get_ui_component (control), verbs, text_view);
	
		gtk_signal_connect (GTK_OBJECT (bonobo_control_get_ui_component (control)),
			    "ui_event", handle_ui_event, text_view);
	}

        /* Note that we do nothing if state is FALSE. Nautilus content
         * views are never explicitly deactivated
	 */
}

/* handle the zoomable signals */
static void
nautilus_text_view_zoom_to_level (NautilusTextView *text_view, int zoom_index)
{
	int pinned_zoom_index;
	pinned_zoom_index = zoom_index;

	if (pinned_zoom_index < 0) {
		pinned_zoom_index = 0;
	} else if (pinned_zoom_index > max_preferred_zoom_levels) {
		pinned_zoom_index = max_preferred_zoom_levels;
	}
	
	if (pinned_zoom_index != text_view->details->zoom_index) {
		text_view->details->zoom_index = pinned_zoom_index;
		bonobo_zoomable_report_zoom_level_changed (text_view->details->zoomable, text_view_preferred_zoom_levels[pinned_zoom_index]);		
		nautilus_text_view_update_font (text_view);		
	}
 }

static void
nautilus_text_view_bump_zoom_level (NautilusTextView *text_view, int increment)
{
	nautilus_text_view_zoom_to_level (text_view, text_view->details->zoom_index + increment);
}

static void
zoomable_zoom_in_callback (BonoboZoomable *zoomable, NautilusTextView *text_view)
{
	nautilus_text_view_bump_zoom_level (text_view, 1);
}

static void
zoomable_zoom_out_callback (BonoboZoomable *zoomable, NautilusTextView *text_view)
{
	nautilus_text_view_bump_zoom_level (text_view, -1);
}

static int
zoom_index_from_float (float zoom_level)
{
	int i;

	for (i = 0; i < max_preferred_zoom_levels; i++) {
		float this, epsilon;

		/* if we're close to a zoom level */
		this = text_view_preferred_zoom_levels [i];
		epsilon = this * 0.01;

		if (zoom_level < this + epsilon)
			return i;
	}

	return max_preferred_zoom_levels;
}

static void
zoomable_set_zoom_level_callback (BonoboZoomable *zoomable, float level, NautilusTextView *view)
{
	nautilus_text_view_zoom_to_level (view, zoom_index_from_float (level));
}

static void
zoomable_zoom_to_fit_callback (BonoboZoomable *zoomable, NautilusTextView *view)
{
	nautilus_text_view_zoom_to_level (view, zoom_index_from_float (1.0));
}
