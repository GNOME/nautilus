/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Ramiro Estrugo <ramiro@eazel.com>
 *
 */

/*
 * nautilus-mozilla-content-view.c - Mozilla content view component.
 *
 * This component uses the gecko layout engine displays a simple label of the URI
 * and demonstrates merging menu items & toolbar buttons. 
 * It should be a good basis for writing out-of-proc content views.
 */

#include <config.h>
#include "nautilus-mozilla-content-view.h"

#include "gtkmozembed.h"

#include <bonobo/bonobo-control.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>

/* A NautilusContentViewFrame's private information. */
struct _NautilusMozillaContentViewDetail {
	char				 *uri;
	GtkWidget			 *mozilla;
	NautilusContentViewFrame	 *view_frame;
};

static void nautilus_mozilla_content_view_initialize_class (NautilusMozillaContentViewClass *klass);
static void nautilus_mozilla_content_view_initialize       (NautilusMozillaContentView      *view);
static void nautilus_mozilla_content_view_destroy          (GtkObject                       *object);
static void mozilla_notify_location_change_callback        (NautilusContentViewFrame        *view_frame,
							    Nautilus_NavigationInfo         *navinfo,
							    NautilusMozillaContentView      *view);
static void mozilla_merge_bonobo_items_callback            (BonoboObject                    *control,
							    gboolean                         state,
							    gpointer                         user_data);


static GtkVBoxClass *parent_class = NULL;

static void
nautilus_mozilla_content_view_initialize_class (NautilusMozillaContentViewClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);

	parent_class = gtk_type_class (GTK_TYPE_VBOX);
	
	object_class->destroy = nautilus_mozilla_content_view_destroy;
}

static void
nautilus_mozilla_content_view_initialize (NautilusMozillaContentView *view)
{
	view->detail = g_new0 (NautilusMozillaContentViewDetail, 1);

	view->detail->uri = NULL;

	/* Conjure up the beast.  May God have mercy on our souls. */
	view->detail->mozilla = gtk_moz_embed_new ();

	gtk_box_pack_start (GTK_BOX (view), view->detail->mozilla, TRUE, TRUE, 1);

	gtk_widget_show (view->detail->mozilla);
	
	view->detail->view_frame = nautilus_content_view_frame_new (GTK_WIDGET (view));
	
	gtk_signal_connect (GTK_OBJECT (view->detail->view_frame), 
			    "notify_location_change",
			    GTK_SIGNAL_FUNC (mozilla_notify_location_change_callback), 
			    view);

	/* 
	 * Get notified when our bonobo control is activated so we
	 * can merge menu & toolbar items into Nautilus's UI.
	 */
        gtk_signal_connect (GTK_OBJECT (nautilus_view_frame_get_bonobo_control
					(NAUTILUS_VIEW_FRAME (view->detail->view_frame))),
                            "activate",
                            mozilla_merge_bonobo_items_callback,
                            view);
	
	gtk_widget_show_all (GTK_WIDGET (view));
}

static void
nautilus_mozilla_content_view_destroy (GtkObject *object)
{
	NautilusMozillaContentView *view;
	
	view = NAUTILUS_MOZILLA_CONTENT_VIEW (object);
	
	bonobo_object_unref (BONOBO_OBJECT (view->detail->view_frame));

	if (view->detail->uri)
		g_free (view->detail->uri);

	g_free (view->detail);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/**
 * nautilus_mozilla_content_view_get_view_frame:
 *
 * Return the NautilusViewFrame object associated with this view; this
 * is needed to export the view via CORBA/Bonobo.
 * @view: NautilusMozillaContentView to get the view_frame from..
 * 
 **/
NautilusContentViewFrame *
nautilus_mozilla_content_view_get_view_frame (NautilusMozillaContentView *view)
{
	return view->detail->view_frame;
}

/**
 * nautilus_mozilla_content_view_load_uri:
 *
 * Load the resource pointed to by the specified URI.
 * @view: NautilusMozillaContentView to get the view_frame from.
 * 
 **/
void
nautilus_mozilla_content_view_load_uri (NautilusMozillaContentView	*view,
					const char			*uri)
{
	g_assert (uri != NULL);

	if (view->detail->uri)
		g_free (view->detail->uri);

	/* FIXME: This is a temporary dumbass hack */
	if (strncmp (uri, "moz:", strlen ("moz:")) == 0)
	{
		view->detail->uri = g_strdup_printf ("http:%s", uri + strlen ("moz:"));
	}
	else
	{
		view->detail->uri = g_strdup (uri);
	}

	g_print ("nautilus_mozilla_content_view_load_uri (%s)\n", view->detail->uri);

	gtk_moz_embed_load_url (view->detail->mozilla, view->detail->uri);
}

static void
mozilla_notify_location_change_callback (NautilusContentViewFrame	*view_frame, 
					 Nautilus_NavigationInfo	*navinfo, 
					 NautilusMozillaContentView	*view)
{
	Nautilus_ProgressRequestInfo request;

	g_assert (view_frame == view->detail->view_frame);
	
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
	nautilus_view_frame_request_progress_change (NAUTILUS_VIEW_FRAME (view_frame), &request);
	
	/* Do the actual load. */
	nautilus_mozilla_content_view_load_uri (view, navinfo->actual_uri);
	
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
	nautilus_view_frame_request_progress_change (NAUTILUS_VIEW_FRAME (view_frame), &request);
}

static void
bonobo_mozilla_callback (BonoboUIHandler *ui_handler, gpointer user_data, const char *path)
{
 	NautilusMozillaContentView *view;
	char *label_text;

        g_assert (NAUTILUS_IS_MOZILLA_CONTENT_VIEW (user_data));

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	if (strcmp (path, "/File/Mozilla") == 0) {
		label_text = g_strdup_printf ("%s\n\nYou selected the Mozilla menu item.", view->detail->uri);
	} else {
		g_assert (strcmp (path, "/Main/Mozilla") == 0);
		label_text = g_strdup_printf ("%s\n\nYou clicked the Mozilla toolbar button.", view->detail->uri);
	}
	
// 	gtk_label_set_text (GTK_LABEL (view->detail->label), label_text);
	g_free (label_text);


	gtk_moz_embed_load_url (view->detail->mozilla, "http://www.gnome.org/");
}

static void
mozilla_merge_bonobo_items_callback (BonoboObject *control, gboolean state, gpointer user_data)
{
 	NautilusMozillaContentView *view;
        BonoboUIHandler *local_ui_handler;

        g_assert (NAUTILUS_IS_MOZILLA_CONTENT_VIEW (user_data));

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);
        local_ui_handler = bonobo_control_get_ui_handler (BONOBO_CONTROL (control));

        if (state) {
        	/* Tell the Nautilus window to merge our bonobo_ui_handler items with its ones */
                bonobo_ui_handler_set_container (local_ui_handler, 
                                                 bonobo_control_get_remote_ui_handler (BONOBO_CONTROL (control)));

                /* 
                 * Create our mozilla menu item.
                 *
                 * Note that it's sorta bogus that we know Nautilus has a menu whose
                 * path is "/File", and also that we know a sensible position to use within
                 * that menu. Nautilus should publish this information somehow.
                 */ 
	        bonobo_ui_handler_menu_new_item (local_ui_handler,				/* BonoboUIHandler */
	        				 "/File/Mozilla",				/* menu item path, must start with /some-existing-menu-path and be otherwise unique */
	                                         _("_Mozilla"),					/* menu item user-displayed label */
	                                         _("This is a mozilla merged menu item"),	/* hint that appears in status bar */
	                                         1,						/* position within menu; -1 means last */
	                                         BONOBO_UI_HANDLER_PIXMAP_NONE,			/* pixmap type */
	                                         NULL,						/* pixmap data */
	                                         'M',						/* accelerator key, couldn't bear the thought of using Control-S for anything except Save */
	                                         GDK_CONTROL_MASK,				/* accelerator key modifiers */
	                                         bonobo_mozilla_callback,			/* callback function */
	                                         view);                				/* callback function data */

                /* 
                 * Create our mozilla toolbar button.
                 *
                 * Note that it's sorta bogus that we know Nautilus has a toolbar whose
                 * path is "/Main". Nautilus should publish this information somehow.
                 */ 
	        bonobo_ui_handler_toolbar_new_item (local_ui_handler,				/* BonoboUIHandler */
	        				    "/Main/Mozilla",				/* button path, must start with /Main/ and be otherwise unique */
	        				    _("Mozilla"),					/* button user-displayed label */
	        				    _("This is a mozilla merged toolbar button"),/* hint that appears in status bar */
	        				    -1,						/* position, -1 means last */
	        				    BONOBO_UI_HANDLER_PIXMAP_STOCK,		/* pixmap type */
	        				    GNOME_STOCK_PIXMAP_BOOK_BLUE,		/* pixmap data */
	        				    0,						/* accelerator key */
	        				    0,						/* accelerator key modifiers */
	        				    bonobo_mozilla_callback,			/* callback function */
	        				    view);					/* callback function's data */
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

GtkType
nautilus_mozilla_content_view_get_type (void)
{
	static GtkType mozilla_content_view_type = 0;
	
	if (!mozilla_content_view_type)
	{
		static const GtkTypeInfo mozilla_content_view_info =
		{
			"NautilusMozillaContentView",
			sizeof (NautilusMozillaContentView),
			sizeof (NautilusMozillaContentViewClass),
			(GtkClassInitFunc) nautilus_mozilla_content_view_initialize_class,
			(GtkObjectInitFunc) nautilus_mozilla_content_view_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		mozilla_content_view_type = gtk_type_unique (GTK_TYPE_VBOX, &mozilla_content_view_info);
	}
	
	return mozilla_content_view_type;
}
