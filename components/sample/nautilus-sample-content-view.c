/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 */

/* nautilus-sample-content-view.c - sample content view
   component. This component displays a simple label of the URI
   and demonstrates merging menu items & toolbar buttons. 
   It should be a good basis for writing out-of-proc content views.
 */

#include <config.h>
#include "nautilus-sample-content-view.h"

#include <bonobo/bonobo-control.h>
#include <gtk/gtksignal.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libnautilus/nautilus-bonobo-ui.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>

struct NautilusSampleContentViewDetails {
	char *location;
	NautilusView *nautilus_view;
};

static void nautilus_sample_content_view_initialize_class (NautilusSampleContentViewClass *klass);
static void nautilus_sample_content_view_initialize       (NautilusSampleContentView      *view);
static void nautilus_sample_content_view_destroy          (GtkObject                      *object);
static void sample_load_location_callback                 (NautilusView                   *nautilus_view,
							   const char                     *location,
							   gpointer                        user_data);
static void sample_merge_bonobo_items_callback            (BonoboControl                  *control,
							   gboolean                        state,
							   gpointer                        user_data);

/* FIXME bugzilla.eazel.com 2410: 
 * Should we use this Nautilus-only macro in a class that's
 * supposed to be a sample?
 */
NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSampleContentView,
				   nautilus_sample_content_view,
				   GTK_TYPE_LABEL)
     
static void
nautilus_sample_content_view_initialize_class (NautilusSampleContentViewClass *klass)
{
	GtkObjectClass *object_class;
	
	g_assert (NAUTILUS_IS_SAMPLE_CONTENT_VIEW_CLASS (klass));

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_sample_content_view_destroy;
}

static void
nautilus_sample_content_view_initialize (NautilusSampleContentView *view)
{
	g_assert (NAUTILUS_IS_SAMPLE_CONTENT_VIEW (view));

	view->details = g_new0 (NautilusSampleContentViewDetails, 1);
	
	gtk_label_set_text (GTK_LABEL (view), g_strdup (_("(none)")));
	
	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));
	
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "load_location",
			    sample_load_location_callback, 
			    view);

	/* Get notified when our bonobo control is activated so we can
	 * merge menu & toolbar items into the shell's UI.
	 */
        gtk_signal_connect (GTK_OBJECT (nautilus_view_get_bonobo_control
					(view->details->nautilus_view)),
                            "activate",
                            sample_merge_bonobo_items_callback,
                            view);
	
	gtk_widget_show (GTK_WIDGET (view));
}

static void
nautilus_sample_content_view_destroy (GtkObject *object)
{
	NautilusSampleContentView *view;
	
	view = NAUTILUS_SAMPLE_CONTENT_VIEW (object);
	
	g_free (view->details->location);
	g_free (view->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/**
 * nautilus_sample_content_view_get_nautilus_view:
 *
 * Return the NautilusView object associated with this view; this
 * is needed to export the view via CORBA/Bonobo.
 *
 * @view: NautilusSampleContentView to get the nautilus_view from..
 **/
NautilusView *
nautilus_sample_content_view_get_nautilus_view (NautilusSampleContentView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_SAMPLE_CONTENT_VIEW (view), NULL);

	return view->details->nautilus_view;
}

static void
load_location (NautilusSampleContentView *view,
	       const char *location)
{
	char *label_text;

	g_assert (NAUTILUS_IS_SAMPLE_CONTENT_VIEW (view));
	g_assert (location != NULL);
	
	g_free (view->details->location);
	view->details->location = g_strdup (location);

	label_text = g_strdup_printf (_("%s\n\nThis is a sample Nautilus content view component."), location);
	gtk_label_set_text (GTK_LABEL (view), label_text);
	g_free (label_text);
}

static void
sample_load_location_callback (NautilusView *nautilus_view, 
			       const char *location,
			       gpointer user_data)
{
	NautilusSampleContentView *view;
	
	g_assert (NAUTILUS_IS_VIEW (nautilus_view));
	g_assert (location != NULL);
	
	view = NAUTILUS_SAMPLE_CONTENT_VIEW (user_data);
	
	g_assert (nautilus_view == view->details->nautilus_view);
	
	/* It's mandatory to send an underway message once the
	 * component starts loading, otherwise nautilus will assume it
	 * failed. In a real component, this will probably happen in
	 * some sort of callback from whatever loading mechanism it is
	 * using to load the data; this component loads no data, so it
	 * gives the progress update here.
	 */
	nautilus_view_report_load_underway (nautilus_view);
	
	/* Do the actual load. */
	load_location (view, location);
	
	/* It's mandatory to call report_load_complete once the
	 * component is done loading successfully, or
	 * report_load_failed if it completes unsuccessfully. In a
	 * real component, this will probably happen in some sort of
	 * callback from whatever loading mechanism it is using to
	 * load the data; this component loads no data, so it gives
	 * the progress update here.
	 */
	nautilus_view_report_load_complete (nautilus_view);
}

static void
bonobo_sample_callback (BonoboUIComponent *ui, gpointer user_data, const char *verb)
{
 	NautilusSampleContentView *view;
	char *label_text;

	g_assert (BONOBO_IS_UI_COMPONENT (ui));
        g_assert (verb != NULL);

	view = NAUTILUS_SAMPLE_CONTENT_VIEW (user_data);

	if (strcmp (verb, "Sample Menu Item") == 0) {
		label_text = g_strdup_printf ("%s\n\nYou selected the Sample menu item.",
					      view->details->location);
	} else {
		g_assert (strcmp (verb, "Sample Dock Item") == 0);
		label_text = g_strdup_printf (_("%s\n\nYou clicked the Sample toolbar button."),
					      view->details->location);
	}
	
	gtk_label_set_text (GTK_LABEL (view), label_text);
	g_free (label_text);
}

static void
sample_merge_bonobo_items_callback (BonoboControl *control, gboolean state, gpointer user_data)
{
 	NautilusSampleContentView *view;
	BonoboUIComponent *ui_component;
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("Sample Menu Item", bonobo_sample_callback),
		BONOBO_UI_VERB ("Sample Dock Item", bonobo_sample_callback),
		BONOBO_UI_VERB_END
	};

	g_assert (BONOBO_IS_CONTROL (control));
	
	view = NAUTILUS_SAMPLE_CONTENT_VIEW (user_data);

	if (state) {
		ui_component = nautilus_view_set_up_ui (view->details->nautilus_view,
							DATADIR,
							"nautilus-sample-content-view-ui.xml",
							"nautilus-sample-content-view");
									
		bonobo_ui_component_add_verb_list_with_data (ui_component, verbs, view);
	}

        /* Note that we do nothing if state is FALSE. Nautilus content
         * views are activated when installed, but never explicitly
         * deactivated. When the view changes to another, the content
         * view object is destroyed, which ends up calling
         * bonobo_ui_handler_unset_container, which removes its merged
         * menu & toolbar items.
	 */
}
