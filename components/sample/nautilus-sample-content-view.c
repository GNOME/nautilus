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

/* A NautilusContentView's private information. */
struct NautilusSampleContentViewDetails {
	char *uri;
	NautilusView *nautilus_view;
};

static void nautilus_sample_content_view_initialize_class (NautilusSampleContentViewClass *klass);
static void nautilus_sample_content_view_initialize       (NautilusSampleContentView      *view);
static void nautilus_sample_content_view_destroy          (GtkObject                      *object);
static void sample_notify_location_change_callback        (NautilusView                   *nautilus_view,
							   Nautilus_NavigationInfo        *navinfo,
							   NautilusSampleContentView      *view);
static void sample_merge_bonobo_items_callback            (BonoboObject                   *control,
							   gboolean                        state,
							   gpointer                        user_data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSampleContentView, nautilus_sample_content_view, GTK_TYPE_LABEL)
     
static void
nautilus_sample_content_view_initialize_class (NautilusSampleContentViewClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_sample_content_view_destroy;
}

static void
nautilus_sample_content_view_initialize (NautilusSampleContentView *view)
{
	view->details = g_new0 (NautilusSampleContentViewDetails, 1);
	
	gtk_label_set_text (GTK_LABEL (view), g_strdup ("(none)"));
	
	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));
	
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "notify_location_change",
			    GTK_SIGNAL_FUNC (sample_notify_location_change_callback), 
			    view);

	/* 
	 * Get notified when our bonobo control is activated so we
	 * can merge menu & toolbar items into Nautilus's UI.
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
	
	bonobo_object_unref (BONOBO_OBJECT (view->details->nautilus_view));
	
	g_free (view->details->uri);
	g_free (view->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/**
 * nautilus_sample_content_view_get_nautilus_view:
 *
 * Return the NautilusView object associated with this view; this
 * is needed to export the view via CORBA/Bonobo.
 * @view: NautilusSampleContentView to get the nautilus_view from..
 * 
 **/
NautilusView *
nautilus_sample_content_view_get_nautilus_view (NautilusSampleContentView *view)
{
	return view->details->nautilus_view;
}

/**
 * nautilus_sample_content_view_load_uri:
 *
 * Load the resource pointed to by the specified URI.
 * 
 **/
void
nautilus_sample_content_view_load_uri (NautilusSampleContentView *view,
				       const char               *uri)
{
	char *label_text;
	
	g_free (view->details->uri);
	view->details->uri = g_strdup (uri);

	label_text = g_strdup_printf (_("%s\n\nThis is a sample Nautilus content view component."), uri);
	gtk_label_set_text (GTK_LABEL (view), label_text);
	g_free (label_text);
}

static void
sample_notify_location_change_callback (NautilusView *nautilus_view, 
				  	Nautilus_NavigationInfo *navinfo, 
				  	NautilusSampleContentView *view)
{
	Nautilus_ProgressRequestInfo request;

	g_assert (nautilus_view == view->details->nautilus_view);
	
	memset(&request, 0, sizeof(request));
	
	/* 
	 * It's mandatory to send a PROGRESS_UNDERWAY message once the
	 * component starts loading, otherwise nautilus will assume it
	 * failed. In a real component, this will probably happen in
	 * some sort of callback from whatever loading mechanism it is
	 * using to load the data; this component loads no data, so it
	 * gives the progrss update here.  
	 */
	
	request.type = Nautilus_PROGRESS_UNDERWAY;
	request.amount = 0.0;
	nautilus_view_request_progress_change (nautilus_view, &request);
	
	/* Do the actual load. */
	nautilus_sample_content_view_load_uri (view, navinfo->actual_uri);
	
	/*
	 * It's mandatory to send a PROGRESS_DONE_OK message once the
	 * component is done loading successfully, or
	 * PROGRESS_DONE_ERROR if it completes unsuccessfully. In a
	 * real component, this will probably happen in some sort of
	 * callback from whatever loading mechanism it is using to
	 * load the data; this component loads no data, so it gives
	 * the progrss upodate here. 
	 */

	request.type = Nautilus_PROGRESS_DONE_OK;
	request.amount = 100.0;
	nautilus_view_request_progress_change (nautilus_view, &request);
}

static void
bonobo_sample_callback (BonoboUIHandler *ui_handler, gpointer user_data, const char *path)
{
 	NautilusSampleContentView *view;
	char *label_text;

        g_assert (NAUTILUS_IS_SAMPLE_CONTENT_VIEW (user_data));

	view = NAUTILUS_SAMPLE_CONTENT_VIEW (user_data);

	if (strcmp (path, "/File/Sample") == 0) {
		label_text = g_strdup_printf ("%s\n\nYou selected the Sample menu item.", view->details->uri);
	} else {
		g_assert (strcmp (path, "/Main/Sample") == 0);
		label_text = g_strdup_printf (_("%s\n\nYou clicked the Sample toolbar button."), view->details->uri);
	}
	
	gtk_label_set_text (GTK_LABEL (view), label_text);
	g_free (label_text);
}

static void
sample_merge_bonobo_items_callback (BonoboObject *control, gboolean state, gpointer user_data)
{
 	NautilusSampleContentView *view;
	BonoboUIHandler *local_ui_handler;
	GdkPixbuf		*pixbuf;
	BonoboUIHandlerPixmapType pixmap_type;
	char *path;
	
	g_assert (NAUTILUS_IS_SAMPLE_CONTENT_VIEW (user_data));

	view = NAUTILUS_SAMPLE_CONTENT_VIEW (user_data);
	local_ui_handler = bonobo_control_get_ui_handler (BONOBO_CONTROL (control));

	if (state) {
		/* Tell the Nautilus window to merge our bonobo_ui_handler items with its ones */
		bonobo_ui_handler_set_container (local_ui_handler, 
                                                 bonobo_control_get_remote_ui_handler (BONOBO_CONTROL (control)));

		/* Load test pixbuf */
		pixbuf = gdk_pixbuf_new_from_file ("/gnome/share/pixmaps/nautilus/i-directory-24.png");		
		if (pixbuf != NULL)
			pixmap_type = BONOBO_UI_HANDLER_PIXMAP_PIXBUF_DATA;
		else
			pixmap_type = BONOBO_UI_HANDLER_PIXMAP_NONE;

	       /* Create our sample menu item. */ 
		path = bonobo_ui_handler_build_path (NULL,
						     "File", 
						     "Sample",
						     NULL);
		bonobo_ui_handler_menu_new_item 
			(local_ui_handler,					/* BonoboUIHandler */
	        	 path,							/* menu item path, must start with /some-existing-menu-path and be otherwise unique */
	                 _("_Sample"),						/* menu item user-displayed label */
	                 _("This is a sample merged menu item"),		/* hint that appears in status bar */
	                 bonobo_ui_handler_menu_get_pos 
	                 	(local_ui_handler, 
	                         NAUTILUS_MENU_PATH_NEW_WINDOW_ITEM) + 1, 	/* position within menu; -1 means last */
	                 pixmap_type,						/* pixmap type */
	                 pixbuf,						/* pixmap data */
	                 'M',							/* accelerator key, couldn't bear the thought of using Control-S for anything except Save */
	                 GDK_CONTROL_MASK,					/* accelerator key modifiers */
	                 bonobo_sample_callback,				/* callback function */
	                 view);                					/* callback function data */
		g_free (path);

                /* Create our sample toolbar button. */ 
                path = bonobo_ui_handler_build_path (NAUTILUS_TOOLBAR_PATH_MAIN_TOOLBAR, 
                				     "Sample",
                				     NULL);
	        bonobo_ui_handler_toolbar_new_item 
	        	(local_ui_handler,				/* BonoboUIHandler */
	        	 path,						/* button path, must start with /some-existing-toolbar-path and be otherwise unique */
	        	 _("Sample"),					/* button user-displayed label */
	        	 _("This is a sample merged toolbar button"),	/* hint that appears in status bar */
	        	 -1,						/* position, -1 means last */
	        	 pixmap_type,					/* pixmap type */
	        	 pixbuf,					/* pixmap data */
	        	 0,						/* accelerator key */
	        	 0,						/* accelerator key modifiers */
	        	 bonobo_sample_callback,			/* callback function */
	        	 view);						/* callback function's data */
	        g_free (path);
	} else {
		/* Do nothing. */
	}

        /* 
         * Note that we do nothing if state is FALSE. Nautilus content views are activated
         * when installed, but never explicitly deactivated. When the view changes to another,
         * the content view object is destroyed, which ends up calling bonobo_ui_handler_unset_container,
         * which removes its merged menu & toolbar items.
         */
}
