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

/* nautilus-content-loser.c - loser content view component. This
   component fails on demand, either controlled by env variables
   during startup, or using toolbar buttons or menu items.  */

#include <config.h>
#include "nautilus-content-loser.h"

#include <bonobo/bonobo-control.h>
#include <gtk/gtksignal.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libnautilus/nautilus-bonobo-ui.h>
#include <eel/eel-gtk-macros.h>
#include <stdio.h>
#include <stdlib.h>

struct NautilusContentLoserDetails {
	char *uri;
	NautilusView *nautilus_view;
};

static void nautilus_content_loser_initialize_class (NautilusContentLoserClass *klass);
static void nautilus_content_loser_initialize       (NautilusContentLoser      *view);
static void nautilus_content_loser_destroy          (GtkObject                 *object);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusContentLoser, nautilus_content_loser, GTK_TYPE_LABEL)
     
static void loser_load_location_callback      (NautilusView         *nautilus_view,
					       const char           *location,
					       NautilusContentLoser *view);
static void loser_merge_bonobo_items_callback (BonoboObject         *control,
					       gboolean              state,
					       gpointer              user_data);
static void nautilus_content_loser_fail       (void);
static void ensure_fail_env                   (void);

static void
nautilus_content_loser_initialize_class (NautilusContentLoserClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_content_loser_destroy;
}

static void
nautilus_content_loser_initialize (NautilusContentLoser *view)
{
	view->details = g_new0 (NautilusContentLoserDetails, 1);
	
	gtk_label_set_text (GTK_LABEL (view), g_strdup ("(none)"));
	
	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));
	
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (loser_load_location_callback), 
			    view);

	/* Get notified when our bonobo control is activated so we
	 * can merge menu & toolbar items into Nautilus's UI.
	 */
        gtk_signal_connect (GTK_OBJECT (nautilus_view_get_bonobo_control
					(view->details->nautilus_view)),
                            "activate",
                            loser_merge_bonobo_items_callback,
                            view);
	
	gtk_widget_show (GTK_WIDGET (view));
}

static void
nautilus_content_loser_destroy (GtkObject *object)
{
	NautilusContentLoser *view;
	
	view = NAUTILUS_CONTENT_LOSER (object);
	
	g_free (view->details->uri);
	g_free (view->details);
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/**
 * nautilus_content_loser_get_nautilus_view:
 *
 * Return the NautilusView object associated with this view; this
 * is needed to export the view via CORBA/Bonobo.
 * @view: NautilusContentLoser to get the nautilus_view from..
 * 
 **/
NautilusView *
nautilus_content_loser_get_nautilus_view (NautilusContentLoser *view)
{
	return view->details->nautilus_view;
}

/**
 * nautilus_content_loser_load_uri:
 *
 * Load the resource pointed to by the specified URI.
 * @view: NautilusContentLoser to get the nautilus_view from.
 * 
 **/
void
nautilus_content_loser_load_uri (NautilusContentLoser *view,
				       const char               *uri)
{
	char *label_text;
	
	g_free (view->details->uri);
	view->details->uri = g_strdup (uri);
	
	label_text = g_strdup_printf (_("%s\n\nThis is a Nautilus content view that fails on demand."), uri);
	gtk_label_set_text (GTK_LABEL (view), label_text);
	g_free (label_text);
}

static void
loser_load_location_callback (NautilusView *nautilus_view, 
			      const char *location,
			      NautilusContentLoser *view)
{
	g_assert (nautilus_view == view->details->nautilus_view);
	
	nautilus_content_loser_maybe_fail ("pre-underway");
	
	/* It's mandatory to call report_load_underway once the
	 * component starts loading, otherwise nautilus will assume it
	 * failed. In a real component, this will probably happen in
	 * some sort of callback from whatever loading mechanism it is
	 * using to load the data; this component loads no data, so it
	 * gives the progress update here.
	 */
	nautilus_view_report_load_underway (nautilus_view);
	
	nautilus_content_loser_maybe_fail ("pre-load");

	/* Do the actual load. */
	nautilus_content_loser_load_uri (view, location);
	
	nautilus_content_loser_maybe_fail ("pre-done");
	
	/* It's mandatory to call report_load_complete once the
	 * component is done loading successfully, or
	 * report_load_failed if it completes unsuccessfully. In a
	 * real component, this will probably happen in some sort of
	 * callback from whatever loading mechanism it is using to
	 * load the data; this component loads no data, so it gives
	 * the progrss upodate here.
	 */
	nautilus_view_report_load_complete (nautilus_view);
	
	nautilus_content_loser_maybe_fail ("post-done");
}

static void
bonobo_loser_callback (BonoboUIComponent *ui, gpointer user_data, const char *verb)
{
        g_assert (NAUTILUS_IS_CONTENT_LOSER (user_data));

	nautilus_content_loser_fail ();
	gtk_label_set_text (GTK_LABEL (user_data), _("You have tried to kill the Content Loser"));
}

static void
loser_merge_bonobo_items_callback (BonoboObject *control, gboolean state, gpointer user_data)
{
 	NautilusContentLoser *view;
	BonoboUIComponent *ui_component;
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("Kill Content Loser", bonobo_loser_callback),
		BONOBO_UI_VERB_END
	};
	
	nautilus_content_loser_maybe_fail ("pre-merge");

	g_assert (NAUTILUS_IS_CONTENT_LOSER (user_data));

	view = NAUTILUS_CONTENT_LOSER (user_data);

	if (state) {
		ui_component = nautilus_view_set_up_ui (view->details->nautilus_view,
							DATADIR,
							"nautilus-content-loser-ui.xml",
							"nautilus-content-loser");

		bonobo_ui_component_add_verb_list_with_data (ui_component, verbs, view);
	} else {
		/* Do nothing. */
	}

	nautilus_content_loser_maybe_fail ("post-merge");


        /* 
         * Note that we do nothing if state is FALSE. Nautilus content views are activated
         * when installed, but never explicitly deactivated. When the view changes to another,
         * the content view object is destroyed, which ends up calling bonobo_ui_handler_unset_container,
         * which removes its merged menu & toolbar items.
         */
}

static char *failure_mode = NULL;
static char *failure_point = NULL;
static gboolean env_checked = FALSE;

void
nautilus_content_loser_maybe_fail (const char *location)
{
	ensure_fail_env ();
	
	if (strcasecmp (location, failure_point) == 0) {
		nautilus_content_loser_fail ();
	}
}
				   


static void
nautilus_content_loser_fail (void)
{
	ensure_fail_env ();
	
	if (strcasecmp (failure_mode, "hang") == 0) {
		while (1) {
		}
	} else if (strcasecmp (failure_mode, "exit") == 0) {
		exit (0);
	} else if (strcasecmp (failure_mode, "error-exit") == 0) {
		exit (-1);
	} else if (strcasecmp (failure_mode, "crash") == 0) {
		abort ();
	} else {
		puts ("XXX - would fail now, if NAUTILUS_CONTENT_LOSER_MODE were set properly.");
	}
}


static void
ensure_fail_env (void)
{
	if (!env_checked) {
		failure_mode = g_getenv ("NAUTILUS_CONTENT_LOSER_MODE");
		if (failure_mode == NULL) {
			failure_mode = "";
		}
		
		failure_point = g_getenv ("NAUTILUS_CONTENT_LOSER_PLACE");
		if (failure_point == NULL) {
			failure_point = "";
		}
		
		env_checked = TRUE;
	}
}
