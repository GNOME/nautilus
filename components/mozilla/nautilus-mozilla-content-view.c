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
 * This component uses the mozilla gecko layout engine via the gtk_moz_embed
 * widget to display and munge html.
 */

//#define DEBUG_ramiro 1

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
	GdkCursor			 *busy_cursor;
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


/* Mozilla embed widget callbacks */
static void mozilla_title_changed_callback                 (GtkWidget                       *widget,
							    gpointer                         user_data);
static void mozilla_location_changed_callback              (GtkWidget                       *widget,
							    gpointer                         user_data);
static void mozilla_net_status_callback                    (GtkWidget                       *widget,
							    gint                             flags,
							    gpointer                         user_data);
static void mozilla_link_message_callback                  (GtkWidget                       *widget,
							    gpointer                         user_data);
static void mozilla_progress_callback                      (GtkWidget                       *widget,
							    gint                             max_progress,
							    gint                             current_progress,
							    gpointer                         user_data);


/* Other mozilla content view functions */
static void mozilla_content_view_set_busy_cursor           (NautilusMozillaContentView      *view);
static void mozilla_content_view_clear_busy_cursor         (NautilusMozillaContentView      *view);
static void mozilla_content_view_send_progress_request     (NautilusMozillaContentView      *view,
							    Nautilus_ProgressType            progress_type,
							    gdouble                          progress_amount);


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

	/* Add callbacks to the beast */
	gtk_signal_connect (GTK_OBJECT (view->detail->mozilla), 
			    "title",
			    GTK_SIGNAL_FUNC (mozilla_title_changed_callback),
			    view);

	gtk_signal_connect (GTK_OBJECT (view->detail->mozilla), 
			    "location",
			    GTK_SIGNAL_FUNC (mozilla_location_changed_callback),
			    view);

	gtk_signal_connect (GTK_OBJECT (view->detail->mozilla), 
			    "net_status",
			    GTK_SIGNAL_FUNC (mozilla_net_status_callback),
			    view);

	gtk_signal_connect (GTK_OBJECT (view->detail->mozilla), 
			    "link_message",
			    GTK_SIGNAL_FUNC (mozilla_link_message_callback),
			    view);

	gtk_signal_connect (GTK_OBJECT (view->detail->mozilla), 
			    "progress",
			    GTK_SIGNAL_FUNC (mozilla_progress_callback),
			    view);

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

	if (view->detail->uri) {
		g_free (view->detail->uri);
	}

	if (view->detail->busy_cursor) {
		gdk_cursor_destroy (view->detail->busy_cursor);
	}

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

	/* FIXME bugzilla.eazel.com 522: This is a temporary dumbass hack */
	if (strncmp (uri, "moz:", strlen ("moz:")) == 0)
	{
		view->detail->uri = g_strdup_printf ("http:%s", uri + strlen ("moz:"));
	}
	else
	{
		view->detail->uri = g_strdup (uri);
	}

#ifdef DEBUG_ramiro
	g_print ("nautilus_mozilla_content_view_load_uri (%s)\n", view->detail->uri);
#endif

	gtk_moz_embed_load_url (GTK_MOZ_EMBED (view->detail->mozilla), view->detail->uri);
}

static void
mozilla_content_view_set_busy_cursor (NautilusMozillaContentView *view)
{
        g_return_if_fail (view != NULL);
        g_return_if_fail (NAUTILUS_IS_MOZILLA_CONTENT_VIEW (view));

	if (!view->detail->busy_cursor) {
		view->detail->busy_cursor = gdk_cursor_new (GDK_WATCH);
	}

	g_assert (view->detail->busy_cursor != NULL);
	g_assert (GTK_WIDGET_REALIZED (GTK_WIDGET (view->detail->mozilla)));

	gdk_window_set_cursor (GTK_WIDGET (view->detail->mozilla)->window, 
			       view->detail->busy_cursor);

	gdk_flush ();
}

static void
mozilla_content_view_send_progress_request (NautilusMozillaContentView	*view,
					    Nautilus_ProgressType	progress_type,
					    gdouble			progress_amount)
{
	Nautilus_ProgressRequestInfo progress_request;

        g_return_if_fail (view != NULL);
        g_return_if_fail (NAUTILUS_IS_MOZILLA_CONTENT_VIEW (view));

	memset (&progress_request, 0, sizeof (progress_request));
	
	progress_request.type = progress_type;
	progress_request.amount = progress_amount;

	nautilus_view_frame_request_progress_change (NAUTILUS_VIEW_FRAME (view->detail->view_frame),
						     &progress_request);
}

static void
mozilla_content_view_clear_busy_cursor (NautilusMozillaContentView *view)
{
        g_return_if_fail (view != NULL);
        g_return_if_fail (NAUTILUS_IS_MOZILLA_CONTENT_VIEW (view));

	g_assert (GTK_WIDGET_REALIZED (GTK_WIDGET (view->detail->mozilla)));
	
	gdk_window_set_cursor (GTK_WIDGET (view->detail->mozilla)->window, NULL);
}

static void
mozilla_notify_location_change_callback (NautilusContentViewFrame	*view_frame, 
					 Nautilus_NavigationInfo	*navinfo, 
					 NautilusMozillaContentView	*view)
{
	g_assert (view_frame == view->detail->view_frame);
	
	/* 
	 * It's mandatory to send a PROGRESS_UNDERWAY message once the
	 * component starts loading, otherwise nautilus will assume it
	 * failed. In a real component, this will probably happen in
	 * some sort of callback from whatever loading mechanism it is
	 * using to load the data; this component loads no data, so it
	 * gives the progrss update here.  
	 */
	mozilla_content_view_send_progress_request (view, Nautilus_PROGRESS_UNDERWAY, 0.0);

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
	mozilla_content_view_send_progress_request (view, Nautilus_PROGRESS_DONE_OK, 100.0);
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
	
	g_free (label_text);


	gtk_moz_embed_load_url (GTK_MOZ_EMBED (view->detail->mozilla), "http://www.gnome.org/");
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

/* Mozilla embed widget callbacks */
static void
mozilla_title_changed_callback (GtkWidget *widget, gpointer user_data)
{
 	NautilusMozillaContentView	*view;
	char				*new_title;

        g_assert (user_data != NULL);
        g_assert (NAUTILUS_IS_MOZILLA_CONTENT_VIEW (user_data));

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	new_title = gtk_moz_embed_get_title (GTK_MOZ_EMBED (view->detail->mozilla));

	nautilus_content_view_frame_request_title_change (NAUTILUS_CONTENT_VIEW_FRAME (view->detail->view_frame),
							  new_title);
	
	g_free (new_title);
}

static void
mozilla_location_changed_callback (GtkWidget *widget, gpointer user_data)
{
 	NautilusMozillaContentView	*view;
	char				*new_location;

        g_assert (user_data != NULL);
        g_assert (NAUTILUS_IS_MOZILLA_CONTENT_VIEW (user_data));

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	new_location = gtk_moz_embed_get_location (GTK_MOZ_EMBED (view->detail->mozilla));

#ifdef DEBUG_ramiro
	g_print ("mozilla_location_changed_callback (%s)\n", new_location);
#endif
	g_free (new_location);
}

static void
mozilla_net_status_callback (GtkWidget *widget, gint flags, gpointer user_data)
{
 	NautilusMozillaContentView	*view;

        g_assert (user_data != NULL);
        g_assert (NAUTILUS_IS_MOZILLA_CONTENT_VIEW (user_data));
	
	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	/* win_start */
	if (flags & gtk_moz_embed_flag_win_start) {
		mozilla_content_view_set_busy_cursor (view);
	}

	/* win_stop */
	if (flags & gtk_moz_embed_flag_win_stop) {
		mozilla_content_view_clear_busy_cursor (view);
	}
	
#ifdef DEBUG_ramiro
 	g_print ("mozilla_net_status_callback (");

	/* net_start */
	if (flags & gtk_moz_embed_flag_net_start) {
		g_print ("net_start ");
	}

	/* net_stop */
	if (flags & gtk_moz_embed_flag_net_stop) {
		g_print ("net_stop ");
	}

	/* net_dns */
	if (flags & gtk_moz_embed_flag_net_dns) {
		g_print ("net_dns ");
	}

	/* net_connecting */
	if (flags & gtk_moz_embed_flag_net_connecting) {
		g_print ("net_connecting ");
	}

	/* net_redirecting */
	if (flags & gtk_moz_embed_flag_net_redirecting) {
		g_print ("net_redirecting ");
	}

	/* net_negotiating */
	if (flags & gtk_moz_embed_flag_net_negotiating) {
		g_print ("net_negotiating ");
	}

	/* net_transferring */
	if (flags & gtk_moz_embed_flag_net_transferring) {
		g_print ("net_transferring ");
	}

	/* net_failedDNS */
	if (flags & gtk_moz_embed_flag_net_failedDNS) {
		g_print ("net_failedDNS ");
	}

	/* net_failedConnect */
	if (flags & gtk_moz_embed_flag_net_failedConnect) {
		g_print ("net_failedConnect ");
	}

	/* net_failedTransfer */
	if (flags & gtk_moz_embed_flag_net_failedTransfer) {
		g_print ("net_failedTransfer ");
	}

	/* net_failedTimeout */
	if (flags & gtk_moz_embed_flag_net_failedTimeout) {
		g_print ("net_failedTimeout ");
	}

	/* net_userCancelled */
	if (flags & gtk_moz_embed_flag_net_userCancelled) {
		g_print ("net_userCancelled ");
	}

	/* win_start */
	if (flags & gtk_moz_embed_flag_win_start) {
		g_print ("win_start ");
	}

	/* win_stop */
	if (flags & gtk_moz_embed_flag_win_stop) {
		g_print ("win_stop ");
	}

 	g_print (")\n");
#endif
}

static void
mozilla_link_message_callback (GtkWidget *widget, gpointer user_data)
{
 	NautilusMozillaContentView	*view;
	Nautilus_StatusRequestInfo	status_request;
	char				*link_message;

        g_assert (user_data != NULL);
        g_assert (NAUTILUS_IS_MOZILLA_CONTENT_VIEW (user_data));

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	link_message = gtk_moz_embed_get_link_message (GTK_MOZ_EMBED (view->detail->mozilla));

	memset (&status_request, 0, sizeof (status_request));

	status_request.status_string = link_message;
	
	nautilus_view_frame_request_status_change (NAUTILUS_VIEW_FRAME (view->detail->view_frame),
						   &status_request);
	g_free (link_message);
}

static void
mozilla_progress_callback (GtkWidget	*widget,
			   gint         max_progress,
			   gint         current_progress,
			   gpointer     user_data)
{
 	NautilusMozillaContentView	*view;
	gdouble				percent;

	g_assert (user_data != NULL);
	g_assert (NAUTILUS_IS_MOZILLA_CONTENT_VIEW (user_data));
	
	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

#ifdef DEBUG_ramiro
	g_print ("mozilla_progress_callback (max = %d, current = %d)\n", max_progress, current_progress);
#endif

	/* The check for 0.0 is just anal paranoia on my part */
	if (max_progress == 0.0) {
		percent = 100.0;
	}
	else {
		percent = current_progress / max_progress;
	}
	
	mozilla_content_view_send_progress_request (view, Nautilus_PROGRESS_UNDERWAY, percent);
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
