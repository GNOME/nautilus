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

#define nopeDEBUG_ramiro 1

#include <config.h>
#include "nautilus-mozilla-content-view.h"

#include "gtkmozembed.h"

#include "mozilla-preferences.h"
#include "mozilla-components.h"
#include "mozilla-events.h"

#include <bonobo/bonobo-control.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomevfs/gnome-vfs.h>
#include <stdlib.h>

struct NautilusMozillaContentViewDetails {
	char				 *uri;
	GtkWidget			 *mozilla;
	NautilusView	                 *nautilus_view;
	GdkCursor			 *busy_cursor;
	gboolean                          got_called_by_nautilus;
};

static const char PROXY_KEY[] = "/system/gnome-vfs/http-proxy";
static const char USE_PROXY_KEY[] = "/system/gnome-vfs/use-http-proxy";

static void     nautilus_mozilla_content_view_initialize_class (NautilusMozillaContentViewClass *klass);
static void     nautilus_mozilla_content_view_initialize       (NautilusMozillaContentView      *view);
static void     nautilus_mozilla_content_view_destroy          (GtkObject                       *object);
static void     mozilla_load_location_callback                 (NautilusView                    *nautilus_view,
								const char                      *location,
								NautilusMozillaContentView      *view);
static void     mozilla_merge_bonobo_items_callback            (BonoboObject                    *control,
								gboolean                         state,
								gpointer                         user_data);

/* Mozilla embed widget callbacks */
static void     mozilla_title_changed_callback                 (GtkMozEmbed                     *mozilla,
								gpointer                         user_data);
static void     mozilla_location_changed_callback              (GtkMozEmbed                     *mozilla,
								gpointer                         user_data);

/*
 * The name of these callbacks changed at M17
 */
#if (MOZILLA_MILESTONE >= 17)
static void     mozilla_net_state_callback                     (GtkMozEmbed                     *mozilla,
								gint                             state,
								guint                            status,
								gpointer                         user_data);
#else
static void     mozilla_net_status_callback                    (GtkMozEmbed                     *mozilla,
								gint                             flags,
								gpointer                         user_data);
#endif
static void     mozilla_link_message_callback                  (GtkMozEmbed                     *mozilla,
								gpointer                         user_data);
static void     mozilla_progress_callback                      (GtkMozEmbed                     *mozilla,
								gint                             max_progress,
								gint                             current_progress,
								gpointer                         user_data);
static  gint    mozilla_open_uri_callback                      (GtkMozEmbed                     *mozilla,
								const char                      *uri,
								gpointer                         user_data);


/*
 * For M18, we peek dom events to deal with eazel: uris (and all other special uris)
 */
#if (MOZILLA_MILESTONE >= 18)
static  gint    mozilla_dom_mouse_click_callback               (GtkMozEmbed                     *mozilla,
								gpointer                         dom_event,
								gpointer                         user_data);
#endif /* MOZILLA_MILESTONE >= 18 */


/* Other mozilla content view functions */
static void     mozilla_content_view_set_busy_cursor           (NautilusMozillaContentView      *view);
static void     mozilla_content_view_clear_busy_cursor         (NautilusMozillaContentView      *view);
static gboolean mozilla_is_uri_handled_by_nautilus             (const char                      *uri);

static GtkVBoxClass *parent_class = NULL;

static void
nautilus_mozilla_content_view_initialize_class (NautilusMozillaContentViewClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);

	parent_class = gtk_type_class (GTK_TYPE_VBOX);
	
	object_class->destroy = nautilus_mozilla_content_view_destroy;
}

static gboolean mozilla_one_time_happenings = FALSE;

static void
nautilus_mozilla_content_view_initialize (NautilusMozillaContentView *view)
{
	GConfClient *gconf_client;
  	GError *error = NULL;
  	char *argv[] = { "nautilus-mozilla-component", NULL };
  	char *proxy_string = NULL;
  	
	view->details = g_new0 (NautilusMozillaContentViewDetails, 1);

	view->details->uri = NULL;

	view->details->got_called_by_nautilus = FALSE;

	/* Conjure up the beast.  May God have mercy on our souls. */
	view->details->mozilla = gtk_moz_embed_new ();

	if (!mozilla_one_time_happenings)
	{
		mozilla_one_time_happenings = TRUE;

		/*
		 * Set some mozilla preferences 
		 */
		mozilla_preference_set_boolean ("nglayout.widget.gfxscrollbars", FALSE);
		mozilla_preference_set_boolean ("security.checkloaduri", FALSE);
		mozilla_preference_set ("general.useragent.misc", "Nautilus");

		/* Locate and set proxy preferences */
		if (gconf_init (1, argv, &error)) {
			gconf_client = gconf_client_get_default ();
			if (gconf_client != NULL) {
				if (gconf_client_get_bool (gconf_client, USE_PROXY_KEY, &error)) {
					proxy_string = gconf_client_get_string (gconf_client, PROXY_KEY, &error);
					if (proxy_string != NULL) {
						char *proxy, *port;						
						port = strchr (proxy_string, ':');
						if (port != NULL) {
							proxy = g_strdup (proxy_string);
							proxy [port - proxy_string] = '\0';
							port++;							

							mozilla_preference_set ("network.proxy.http", proxy);
							mozilla_preference_set_int ("network.proxy.http_port", atoi (port));

							/* 1, Configure proxy settings manually */
							mozilla_preference_set_int ("network.proxy.type", 1);
							
							g_free (proxy_string);
							g_free (proxy);
						}
					}
				} else {
					/* Default is 3, which conects to internet hosts directly */
					mozilla_preference_set_int ("network.proxy.type", 3);
				}
				gtk_object_unref (GTK_OBJECT (gconf_client));
			}
		}
/*
 * For M17, we register a necko protocol handler to deal with eazel: uris
 */
#if (MOZILLA_MILESTONE <= 17)
		/*
		 * Register some mozilla xpcom components
		 */
		{
			gboolean	result;
			const char	*class_uuid = "{fb21b992-1dd1-11b2-aab4-906384831dd4}";
			
#ifdef MOZILLA_EAZEL_PROTOCOL_HANDLER_COMPONENT_PATH
			const char	*library_file_name = MOZILLA_EAZEL_PROTOCOL_HANDLER_COMPONENT_PATH;
#else
			const char	*library_file_name = "you lose";
#endif
			const char	*class_name = "The Eazel Protocol Handler";
			const char	*prog_id = "component://netscape/network/protocol?name=eazel";
			
			result = mozilla_components_register_library (class_uuid,
								      library_file_name,
								      class_name,
								      prog_id);
			
			result = mozilla_components_register_library (class_uuid,
								      library_file_name,
								      class_name,
								      "component://netscape/network/protocol?name=eazel-summary");
			
			if (!result) {
				g_warning ("Unable to register the eazel protocol handler.\n");
			}
		}
#endif /* MOZILLA_MILESTONE <= 17 */
	}
	
	/* Add callbacks to the beast */
	gtk_signal_connect_while_alive (GTK_OBJECT (view->details->mozilla), 
					"title",
					GTK_SIGNAL_FUNC (mozilla_title_changed_callback),
					view,
					GTK_OBJECT (view->details->mozilla));

	gtk_signal_connect_while_alive (GTK_OBJECT (view->details->mozilla), 
					"location",
					GTK_SIGNAL_FUNC (mozilla_location_changed_callback),
					view,
					GTK_OBJECT (view->details->mozilla));
	
/*
 * The name of these callbacks changed at M17
 */
#if (MOZILLA_MILESTONE >= 17)
	gtk_signal_connect_while_alive (GTK_OBJECT (view->details->mozilla), 
					"net_state",
					GTK_SIGNAL_FUNC (mozilla_net_state_callback),
					view,
					GTK_OBJECT (view->details->mozilla));
#else
	gtk_signal_connect_while_alive (GTK_OBJECT (view->details->mozilla), 
					"net_status",
					GTK_SIGNAL_FUNC (mozilla_net_status_callback),
					view,
					GTK_OBJECT (view->details->mozilla));
#endif
	
	gtk_signal_connect_while_alive (GTK_OBJECT (view->details->mozilla), 
					"link_message",
					GTK_SIGNAL_FUNC (mozilla_link_message_callback),
					view,
					GTK_OBJECT (view->details->mozilla));
	
	gtk_signal_connect_while_alive (GTK_OBJECT (view->details->mozilla), 
					"progress",
					GTK_SIGNAL_FUNC (mozilla_progress_callback),
					view,
					GTK_OBJECT (view->details->mozilla));
	
	gtk_signal_connect_while_alive (GTK_OBJECT (view->details->mozilla), 
					"open_uri",
					GTK_SIGNAL_FUNC (mozilla_open_uri_callback),
					view,
					GTK_OBJECT (view->details->mozilla));
	
/*
 * For M18, we peek dom events to deal with eazel: uris (and all other special uris)
 */
#if (MOZILLA_MILESTONE >= 18)
	gtk_signal_connect_while_alive (GTK_OBJECT (view->details->mozilla), 
					"dom_mouse_click",
					GTK_SIGNAL_FUNC (mozilla_dom_mouse_click_callback),
					view,
					GTK_OBJECT (view->details->mozilla));
#endif /* MOZILLA_MILESTONE >= 18 */
	
	gtk_box_pack_start (GTK_BOX (view), view->details->mozilla, TRUE, TRUE, 1);
	
	gtk_widget_show (view->details->mozilla);
	
	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));
	
	gtk_signal_connect_while_alive (GTK_OBJECT (view->details->nautilus_view), 
					"load_location",
					GTK_SIGNAL_FUNC (mozilla_load_location_callback), 
					view,
					GTK_OBJECT (view->details->mozilla));

	/* 
	 * Get notified when our bonobo control is activated so we
	 * can merge menu & toolbar items into Nautilus's UI.
	 */
        gtk_signal_connect_while_alive (
		GTK_OBJECT (nautilus_view_get_bonobo_control (view->details->nautilus_view)),
		"activate",
		mozilla_merge_bonobo_items_callback,
		view,
		GTK_OBJECT (view->details->mozilla));
	
	gtk_widget_show_all (GTK_WIDGET (view));
}

static void
nautilus_mozilla_content_view_destroy (GtkObject *object)
{
	NautilusMozillaContentView *view;

#ifdef DEBUG_ramiro
	g_print ("%s()\n", __FUNCTION__);
#endif

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (object);
	
	g_free (view->details->uri);

	if (view->details->busy_cursor != NULL) {
		gdk_cursor_destroy (view->details->busy_cursor);
	}

	g_free (view->details);

	if (GTK_OBJECT_CLASS (parent_class)->destroy) {
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
	}
}

/**
 * nautilus_mozilla_content_view_get_nautilus_view:
 *
 * Return the NautilusView object associated with this view; this
 * is needed to export the view via CORBA/Bonobo.
 * @view: NautilusMozillaContentView to get the nautilus_view from..
 * 
 **/
NautilusView *
nautilus_mozilla_content_view_get_nautilus_view (NautilusMozillaContentView *view)
{
	return view->details->nautilus_view;
}


/**
 * mozilla_vfs_read_callback:
 *
 * Read data from buffer and copy into mozilla stream.
 **/

static char vfs_read_buf[40960];

static void
mozilla_vfs_read_callback (GnomeVFSAsyncHandle *handle, GnomeVFSResult result, gpointer buffer,
			   GnomeVFSFileSize bytes_requested,
			   GnomeVFSFileSize bytes_read,
			   gpointer data)
{
	NautilusMozillaContentView *view = data;

#ifdef DEBUG_ramiro
	g_print ("mozilla_vfs_read_callback: %ld/%ld bytes\n", (long) bytes_read, (long) bytes_requested);
#endif
	nautilus_view_report_load_underway (view->details->nautilus_view);
	if (bytes_read != 0) {
		gtk_moz_embed_append_data (GTK_MOZ_EMBED (view->details->mozilla), buffer, bytes_read);
	}

	if (bytes_read == 0 || result != GNOME_VFS_OK) {
		gtk_moz_embed_close_stream (GTK_MOZ_EMBED (view->details->mozilla));
		gnome_vfs_async_close (handle, (GnomeVFSAsyncCloseCallback) gtk_true, NULL);
		nautilus_view_report_load_complete (view->details->nautilus_view);
		return;
    	}
	
	gnome_vfs_async_read (handle, vfs_read_buf, sizeof (vfs_read_buf), mozilla_vfs_read_callback, view);
}

/**
 * mozilla_vfs_callback:
 *
 * Callback for gnome_vfs_async_open. Attempt to read data from handle
 * and pass to mozilla streaming callback.
 * 
 **/
static void
mozilla_vfs_callback (GnomeVFSAsyncHandle *handle, GnomeVFSResult result, gpointer data)
{
	NautilusMozillaContentView *view = data;

#ifdef DEBUG_ramiro
	g_print ("mozilla_vfs_callback, result was %s\n", gnome_vfs_result_to_string (result));
#endif

	if (result != GNOME_VFS_OK)
	{
		nautilus_view_report_load_failed (view->details->nautilus_view);
		gtk_moz_embed_close_stream (GTK_MOZ_EMBED (view->details->mozilla));
	} else {
		gtk_moz_embed_open_stream (GTK_MOZ_EMBED (view->details->mozilla), "file://", "text/html");
		gnome_vfs_async_read (handle, vfs_read_buf, sizeof (vfs_read_buf), mozilla_vfs_read_callback, view);
	}
}
/**
 * nautilus_mozilla_content_view_load_uri:
 *
 * Load the resource pointed to by the specified URI.
 * @view: NautilusMozillaContentView to get the nautilus_view from.
 * 
 **/
void
nautilus_mozilla_content_view_load_uri (NautilusMozillaContentView	*view,
					const char			*uri)
{
	GnomeVFSAsyncHandle *async_handle;

	g_assert (uri != NULL);

	view->details->got_called_by_nautilus = TRUE;

	if (view->details->uri) {
		g_free (view->details->uri);
	}
	
	view->details->uri = g_strdup (uri);

#ifdef DEBUG_ramiro
	g_print ("nautilus_mozilla_content_view_load_uri (%s)\n", view->details->uri);
#endif
	/* If the request is http, pass the uri to the mozilla component.  Otherwise, stream the data
	 * into mozilla.
	 */
	if (strncmp (uri, "http:", 5) == 0) {
		gtk_moz_embed_load_url (GTK_MOZ_EMBED (view->details->mozilla), view->details->uri);
	} else {
		nautilus_view_report_load_underway (view->details->nautilus_view);
		gnome_vfs_async_open (&async_handle, uri, GNOME_VFS_OPEN_READ, mozilla_vfs_callback, view);	
	}

}

static void
mozilla_content_view_set_busy_cursor (NautilusMozillaContentView *view)
{
        g_return_if_fail (NAUTILUS_IS_MOZILLA_CONTENT_VIEW (view));

	if (!view->details->busy_cursor) {
		view->details->busy_cursor = gdk_cursor_new (GDK_WATCH);
	}

	g_assert (view->details->busy_cursor != NULL);
	g_assert (GTK_WIDGET_REALIZED (GTK_WIDGET (view->details->mozilla)));

	gdk_window_set_cursor (GTK_WIDGET (view->details->mozilla)->window, 
			       view->details->busy_cursor);

	gdk_flush ();
}

static void
mozilla_content_view_clear_busy_cursor (NautilusMozillaContentView *view)
{
        g_return_if_fail (NAUTILUS_IS_MOZILLA_CONTENT_VIEW (view));

	g_assert (GTK_WIDGET_REALIZED (GTK_WIDGET (view->details->mozilla)));
	
	gdk_window_set_cursor (GTK_WIDGET (view->details->mozilla)->window, NULL);
}

static void
mozilla_load_location_callback (NautilusView *nautilus_view, 
				const char *location,
				NautilusMozillaContentView *view)
{
	g_assert (nautilus_view == view->details->nautilus_view);
	
	nautilus_view_report_load_underway (nautilus_view);
	nautilus_mozilla_content_view_load_uri (view, location);
}

#ifdef UIH

static void
bonobo_mozilla_callback (BonoboUIHandler *ui_handler, gpointer user_data, const char *path)
{
 	NautilusMozillaContentView *view;
	char *label_text;

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	if (strcmp (path, "/File/Mozilla") == 0) {
		label_text = g_strdup_printf ("%s\n\nYou selected the Mozilla menu item.", view->details->uri);
	} else {
		g_assert (strcmp (path, "/Main/Mozilla") == 0);
		label_text = g_strdup_printf ("%s\n\nYou clicked the Mozilla toolbar button.", view->details->uri);
	}
	
	g_free (label_text);

#if 0
	/*
	 * This is an example of how you could redirect Nautilus to a component:
	 */
	nautilus_view_open_location (view->details->nautilus_view, "eazel-summary:");
#endif


#if 0
	/*
	 * This is an example of how you could stream stuff into the current mozilla widget:
	 */
	{
		char *text1 = "<html>";
		char *text2 = "<a href=\"http://www.gnome.org\">http://www.gnome.org</a>";
		char *text3 = "</html>";
		
		gtk_moz_embed_open_stream (GTK_MOZ_EMBED(view->details->mozilla), "file://", "text/html");

		gtk_moz_embed_append_data (GTK_MOZ_EMBED(view->details->mozilla), text1, strlen (text1));
		gtk_moz_embed_append_data (GTK_MOZ_EMBED(view->details->mozilla), text2, strlen (text2));
		gtk_moz_embed_append_data (GTK_MOZ_EMBED(view->details->mozilla), text3, strlen (text3));

		gtk_moz_embed_close_stream (GTK_MOZ_EMBED(view->details->mozilla));
	}
#endif


#if 0
	/*
	 * This is an example of how you could make the current mozilla widget load a url:
	 */

	gtk_moz_embed_load_url (GTK_MOZ_EMBED (view->details->mozilla), "http://www.gnome.org/");
#endif

}

#endif

static void
mozilla_merge_bonobo_items_callback (BonoboObject *control, gboolean state, gpointer user_data)
{
 	NautilusMozillaContentView *view;
        BonoboUIComponent *ui_component;

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

        if (state) {
		/* Load the UI from the XML file. */
		ui_component = nautilus_view_set_up_ui (view->details->nautilus_view,
							DATADIR,
							"nautilus-mozilla-ui.xml",
							"nautilus-mozilla");

#ifdef UIH
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
#endif
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
mozilla_title_changed_callback (GtkMozEmbed *mozilla, gpointer user_data)
{
 	NautilusMozillaContentView	*view;
	char				*new_title;

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	g_assert (GTK_MOZ_EMBED (mozilla) == GTK_MOZ_EMBED (view->details->mozilla));

	new_title = gtk_moz_embed_get_title (GTK_MOZ_EMBED (view->details->mozilla));

	if (new_title != NULL && strcmp (new_title, "") != 0) {
		nautilus_view_set_title (view->details->nautilus_view,
					 new_title);
	}
	
	g_free (new_title);
}

static gboolean
mozilla_uris_differ_only_by_fragment_identifier (const char *uri1, const char *uri2)
{
	char *uri1_hash;
	char *uri2_hash;

	uri1_hash = strchr (uri1, '#');
	uri2_hash = strchr (uri2, '#');

	if (uri1_hash == NULL && uri2_hash == NULL) {
		/* neither has a fragment identifier - return true
		 * only if they match exactly 
		 */
		return (strcasecmp (uri1, uri2) == 0);
	}

	if (uri1_hash == NULL) {
		/* only the second has an fragment identifier - return
		 * true only if the part before the fragment
		 * identifier matches the first URI 
		 */
		return (strncasecmp (uri1, uri2, uri2_hash - uri2) == 0);
	}

	if (uri2_hash == NULL) {
		/* only the first has a fragment identifier - return
		 * true only if the part before the fragment
		 * identifier matches the second URI 
		 */
		return (strncasecmp (uri1, uri2, uri1_hash - uri1) == 0);
	}

	if (uri1_hash - uri1 == uri2_hash - uri2) {
		/* both have a fragment identifier - return true only
		 * if the parts before the fragment identifier match
		 */
		return (strncasecmp (uri1, uri2, uri1_hash - uri1) == 0);
	}
	
	return FALSE;
}

static void
mozilla_location_changed_callback (GtkMozEmbed *mozilla, gpointer user_data)
{
 	NautilusMozillaContentView	*view;
	char				*new_location;


	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	g_assert (GTK_MOZ_EMBED (mozilla) == GTK_MOZ_EMBED (view->details->mozilla));

	new_location = gtk_moz_embed_get_location (GTK_MOZ_EMBED (view->details->mozilla));

#ifdef DEBUG_ramiro
	g_print ("mozilla_location_changed_callback (%s)\n", new_location);
#endif

	/* If we only changed anchors in the same page, we should
           report some fake progress to Nautilus. */

	if (mozilla_uris_differ_only_by_fragment_identifier (view->details->uri,
							     new_location)) {
		nautilus_view_report_load_underway (view->details->nautilus_view);
		nautilus_view_report_load_complete (view->details->nautilus_view);
	}

	if (view->details->uri) {
		g_free (view->details->uri);
	}
	
	view->details->uri = new_location;
}

#if defined(DEBUG_ramiro) && (MOZILLA_MILESTONE >= 17)

#define PRINT_FLAG(bits, mask, message)		\
G_STMT_START {					\
  if ((bits) & (mask)) {			\
	  g_print ("%s ", (message));		\
  }						\
} G_STMT_END

static void
debug_print_state_flags (gint state_flags)
{
	g_print ("state_flags = ");
	PRINT_FLAG (state_flags, GTK_MOZ_EMBED_FLAG_START, "start");
	PRINT_FLAG (state_flags, GTK_MOZ_EMBED_FLAG_REDIRECTING, "redirecting");
	PRINT_FLAG (state_flags, GTK_MOZ_EMBED_FLAG_TRANSFERRING, "transferring");
	PRINT_FLAG (state_flags, GTK_MOZ_EMBED_FLAG_NEGOTIATING, "negotiating");
	PRINT_FLAG (state_flags, GTK_MOZ_EMBED_FLAG_STOP, "stop");
	PRINT_FLAG (state_flags, GTK_MOZ_EMBED_FLAG_IS_REQUEST, "is_request");
	PRINT_FLAG (state_flags, GTK_MOZ_EMBED_FLAG_IS_DOCUMENT, "is_document");
	PRINT_FLAG (state_flags, GTK_MOZ_EMBED_FLAG_IS_NETWORK, "is_network");
	PRINT_FLAG (state_flags, GTK_MOZ_EMBED_FLAG_IS_WINDOW, "is_window");
	g_print ("\n");
}

static void
debug_print_status_flags (guint status_flags)
{
	g_print ("status_flags = ");
	PRINT_FLAG (status_flags, GTK_MOZ_EMBED_STATUS_FAILED_DNS, "failed_dns");
	PRINT_FLAG (status_flags, GTK_MOZ_EMBED_STATUS_FAILED_CONNECT, "failed_connect");
	PRINT_FLAG (status_flags, GTK_MOZ_EMBED_STATUS_FAILED_TIMEOUT, "failed_timeout");
	PRINT_FLAG (status_flags, GTK_MOZ_EMBED_STATUS_FAILED_USERCANCELED, "failed_usercanceled");
	g_print ("\n");
}


#endif


#if (MOZILLA_MILESTONE >= 17)
static void
mozilla_net_state_callback (GtkMozEmbed	*mozilla,
			    gint	state_flags,
			    guint	status_flags,
			    gpointer	user_data)
{
 	NautilusMozillaContentView	*view;

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	g_assert (GTK_MOZ_EMBED (mozilla) == GTK_MOZ_EMBED (view->details->mozilla));

#if defined(DEBUG_ramiro) && (MOZILLA_MILESTONE >= 17)
	g_print ("%s\n", __FUNCTION__);
	debug_print_state_flags (state_flags);
	debug_print_status_flags (status_flags);
	g_print ("\n\n");
#endif

	/* win_start */
	if (state_flags & GTK_MOZ_EMBED_FLAG_START) {
		mozilla_content_view_set_busy_cursor (view);
	}

	/* win_stop */
	if (state_flags & GTK_MOZ_EMBED_FLAG_STOP) {
		mozilla_content_view_clear_busy_cursor (view);
	}
}
#else
static void
mozilla_net_status_callback (GtkMozEmbed *mozilla, gint flags, gpointer user_data)
{
 	NautilusMozillaContentView	*view;

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	g_assert (GTK_MOZ_EMBED (mozilla) == GTK_MOZ_EMBED (view->details->mozilla));

	/* win_start */
	if (flags & GTK_MOZ_EMBED_FLAG_WIN_START) {
		mozilla_content_view_set_busy_cursor (view);
	}

	/* win_stop */
	if (flags & GTK_MOZ_EMBED_FLAG_WIN_STOP) {
		mozilla_content_view_clear_busy_cursor (view);
	}
}
#endif

static void
mozilla_link_message_callback (GtkMozEmbed *mozilla, gpointer user_data)
{
 	NautilusMozillaContentView	*view;
	char				*link_message;

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	g_assert (GTK_MOZ_EMBED (mozilla) == GTK_MOZ_EMBED (view->details->mozilla));

	link_message = gtk_moz_embed_get_link_message (GTK_MOZ_EMBED (view->details->mozilla));
	nautilus_view_report_status (view->details->nautilus_view,
				     link_message);
	g_free (link_message);
}

static void
mozilla_progress_callback (GtkMozEmbed *mozilla,
			   gint         max_progress,
			   gint         current_progress,
			   gpointer     user_data)
{
 	NautilusMozillaContentView	*view;

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);
	g_assert (GTK_MOZ_EMBED (mozilla) == GTK_MOZ_EMBED (view->details->mozilla));

#ifdef DEBUG_ramiro
	g_print ("mozilla_progress_callback (max = %d, current = %d)\n", max_progress, current_progress);
#endif

	if (max_progress == current_progress || max_progress == 0) {
		nautilus_view_report_load_complete (view->details->nautilus_view);
	} else {
		nautilus_view_report_load_progress (view->details->nautilus_view, 
						    current_progress / max_progress);
	}
}


static gint
mozilla_open_uri_callback (GtkMozEmbed *mozilla,
			   const char	*uri,
			   gpointer	user_data)
{
	gint                            abort_uri_open;
 	NautilusMozillaContentView     *view;
	static gboolean                 do_nothing = FALSE;

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	g_assert (GTK_MOZ_EMBED (mozilla) == GTK_MOZ_EMBED (view->details->mozilla));

	/* Determine whether we want to abort this uri load */
	abort_uri_open = mozilla_is_uri_handled_by_nautilus (uri);

#ifdef DEBUG_ramiro
	g_print ("mozilla_open_uri_callback (uri = %s) abort = %s\n",
		 uri,
		 abort_uri_open ? "TRUE" : "FALSE");
#endif

	if (do_nothing == TRUE) {
		do_nothing = FALSE;
		view->details->got_called_by_nautilus = FALSE;
	} else if (view->details->got_called_by_nautilus == TRUE) {
		view->details->got_called_by_nautilus = FALSE;
	} else {
		/* set it so that we load next time we are called. */
	        do_nothing = TRUE;
		abort_uri_open = TRUE;
		bonobo_object_ref (BONOBO_OBJECT (view->details->nautilus_view));
		nautilus_view_open_location (view->details->nautilus_view, uri);
		bonobo_object_unref (BONOBO_OBJECT (view->details->nautilus_view));
	}

	return abort_uri_open;
}

/*
 * For M18, we peek dom events to deal with eazel: uris (and all other special uris)
 */
#if (MOZILLA_MILESTONE >= 18)
static gint
mozilla_dom_mouse_click_callback (GtkMozEmbed *mozilla,
				  gpointer	dom_event,
				  gpointer	user_data)
{
 	NautilusMozillaContentView	*view;
	char				*href;

	g_return_val_if_fail (GTK_IS_MOZ_EMBED (mozilla), TRUE);
	g_return_val_if_fail (dom_event != NULL, TRUE);
	g_return_val_if_fail (NAUTILUS_IS_MOZILLA_CONTENT_VIEW (user_data), TRUE);
	
	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	g_return_val_if_fail (GTK_MOZ_EMBED (mozilla) == GTK_MOZ_EMBED (view->details->mozilla), TRUE);

#ifdef DEBUG_ramiro
	g_print ("%s (%p)\n", __FUNCTION__, dom_event);
#endif

	href = mozilla_events_get_href_for_mouse_event (dom_event);

	/* 
	 * The return value over here needs to be psychoanlized some.
	 * In theory, a return value of NS_OK, which is 0 (yes, zero)
	 * in the Mozilla universe means that the dom event got handled
	 *
	 * Need to look at the gtkmozembed.cpp code more carefully.
	 */

	if (href && mozilla_is_uri_handled_by_nautilus (href)) {
#ifdef DEBUG_ramiro
		g_print ("%s() href = %s\n", __FUNCTION__, href);
#endif
		bonobo_object_ref (BONOBO_OBJECT (view->details->nautilus_view));
		nautilus_view_open_location (view->details->nautilus_view, href);
		bonobo_object_unref (BONOBO_OBJECT (view->details->nautilus_view));
		g_free (href);

		return 0;
	}
	
	return TRUE;
}
#endif /* MOZILLA_MILESTONE >= 18 */

/*
 * The issue here is that mozilla handles some protocols "natively" just as nautilus
 * does thanks to gnome-vfs magic.
 *
 * The embedded mozilla beast provides a mechanism for aborting url loading (ie, the
 * thing that happens when a user boinks on a hyperlink).
 *
 * We use this feature to abort uri loads for the following protocol(s):
 *
 */
static char *handled_by_nautilus[] =
{
	"ftp",
	"eazel",
	"eazel",
	"eazel-install",
	"eazel-pw",
	"eazel-services",
	"man",
	"info",
	"help"
};

#define num_handled_by_nautilus (sizeof (handled_by_nautilus) / sizeof ((handled_by_nautilus)[0]))

static gboolean
mozilla_is_uri_handled_by_nautilus (const char *uri)
{
	guint i;

	g_return_val_if_fail (uri != NULL, TRUE);
	
	for (i = 0; i < num_handled_by_nautilus; i++) {
		if (strlen (uri) >= strlen (handled_by_nautilus[i]) 
		    && (strncmp (uri, handled_by_nautilus[i], strlen (handled_by_nautilus[i])) == 0)) {
			return TRUE;
		}
	}
	
	return FALSE;
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
