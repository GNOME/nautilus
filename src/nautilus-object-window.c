/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *  	     John Sullivan <sullivan@eazel.com>
 *
 */

/* nautilus-window.c: Implementation of the main window object */

#include <config.h>
#include "nautilus-window-private.h"

#include "nautilus-window-manage-views.h"
#include "nautilus-application.h"

#include <gnome.h>
#include <math.h>
#include "nautilus-bookmarks-window.h"
#include "nautilus-signaller.h"
#include "nautilus-location-bar.h"
#include "nautilus-sidebar.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libnautilus/nautilus-bonobo-ui.h>

#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-mime-actions.h>
#include <libnautilus-extensions/nautilus-program-choosing.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-mini-icon.h>
#include <libnautilus-extensions/nautilus-generous-bin.h>
#include <libnautilus/nautilus-undo.h>
#include "nautilus-zoom-control.h"
#include <ctype.h>
#include <libgnomevfs/gnome-vfs-uri.h>

/* FIXME bugzilla.eazel.com 1243: 
 * We should use inheritance instead of these special cases
 * for the desktop window.
 */
#include "nautilus-desktop-window.h"

static void nautilus_window_realize (GtkWidget *widget);
static void nautilus_window_real_set_content_view (NautilusWindow *window, NautilusViewFrame *new_view);

/* Object framework static variables */
static GnomeAppClass *parent_class = NULL;

/* Other static variables */
static GSList *history_list = NULL;

/* Private functions */
static void               nautilus_window_class_init                    (NautilusWindowClass    *klass);
static void               nautilus_window_init                          (NautilusWindow         *window);
static void               nautilus_window_destroy                       (NautilusWindow         *window);
static void               nautilus_window_set_arg                       (GtkObject              *object,
									 GtkArg                 *arg,
									 guint                   arg_id);
static void               nautilus_window_get_arg                       (GtkObject              *object,
									 GtkArg                 *arg,
									 guint                   arg_id);
static void               nautilus_window_goto_uri_callback             (GtkWidget              *widget,
									 const char             *uri,
									 GtkWidget              *window);
static void               zoom_in_callback                              (NautilusZoomControl    *zoom_control,
									 NautilusWindow         *window);
static void               zoom_out_callback                             (NautilusZoomControl    *zoom_control,
									 NautilusWindow         *window);
static void               zoom_to_level_callback                        (NautilusZoomControl    *zoom_control,
									 gint			level,
									 NautilusWindow         *window);
static void               zoom_default_callback                         (NautilusZoomControl    *zoom_control,
									 NautilusWindow         *window);
static void               sidebar_panels_changed_callback               (gpointer                user_data);
static NautilusViewFrame *window_find_sidebar_panel_by_identifier       (NautilusWindow         *window,
									 NautilusViewIdentifier *identifier);
static void               window_update_sidebar_panels_from_preferences (NautilusWindow         *window);

/* Milliseconds */
#define STATUSBAR_CLEAR_TIMEOUT 5000

GtkType
nautilus_window_get_type(void)
{
	static guint window_type = 0;
	
	if (!window_type) {
		GtkTypeInfo window_info = {
			"NautilusWindow",
			sizeof(NautilusWindow),
			sizeof(NautilusWindowClass),
			(GtkClassInitFunc) nautilus_window_class_init,
			(GtkObjectInitFunc) nautilus_window_init,
			NULL,
			NULL,
			NULL
		};
		
		window_type = gtk_type_unique (gnome_app_get_type(), &window_info);
	}
	
	return window_type;
}

enum {
	ARG_0,
	ARG_APP_ID,
	ARG_APP,
	ARG_CONTENT_VIEW
};

static void
nautilus_window_class_init (NautilusWindowClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	parent_class = gtk_type_class(gnome_app_get_type());
	
	object_class = (GtkObjectClass*) klass;
	object_class->destroy = (void (*)(GtkObject *))nautilus_window_destroy;
	object_class->get_arg = nautilus_window_get_arg;
	object_class->set_arg = nautilus_window_set_arg;
	
	widget_class = (GtkWidgetClass*) klass;
	klass->parent_class = gtk_type_class (gtk_type_parent (object_class->type));
	
	gtk_object_add_arg_type ("NautilusWindow::app_id",
				 GTK_TYPE_STRING,
				 GTK_ARG_READWRITE|GTK_ARG_CONSTRUCT,
				 ARG_APP_ID);
	gtk_object_add_arg_type ("NautilusWindow::app",
				 GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE|GTK_ARG_CONSTRUCT,
				 ARG_APP);
	gtk_object_add_arg_type ("NautilusWindow::content_view",
				 GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE,
				 ARG_CONTENT_VIEW);
	
	widget_class->realize = nautilus_window_realize;
}

static void
nautilus_window_init (NautilusWindow *window)
{
	gtk_quit_add_destroy (1, GTK_OBJECT (window));
	
	/* Keep track of sidebar panel changes */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_SIDEBAR_PANELS_NAMESPACE,
					   sidebar_panels_changed_callback,
					   window);
}

static gboolean
nautilus_window_clear_status(NautilusWindow *window)
{
	gtk_statusbar_pop(GTK_STATUSBAR(GNOME_APP(window)->statusbar), window->statusbar_ctx);
	window->statusbar_clear_id = 0;
	return FALSE;
}

void
nautilus_window_set_status(NautilusWindow *window, const char *txt)
{
	if(window->statusbar_clear_id)
		g_source_remove(window->statusbar_clear_id);
	
	gtk_statusbar_pop(GTK_STATUSBAR(GNOME_APP(window)->statusbar), window->statusbar_ctx);
	if(txt && *txt)	{
		window->statusbar_clear_id = g_timeout_add(STATUSBAR_CLEAR_TIMEOUT, (GSourceFunc)nautilus_window_clear_status, window);
		gtk_statusbar_push(GTK_STATUSBAR(GNOME_APP(window)->statusbar), window->statusbar_ctx, txt);
	} else
		  window->statusbar_clear_id = 0;
}

void
nautilus_window_goto_uri (NautilusWindow *window, const char *uri)
{
	nautilus_window_open_location (window, uri, NULL);
}

static void
nautilus_window_goto_uri_callback (GtkWidget *widget,
				   const char *uri,
				   GtkWidget *window)
{
	nautilus_window_goto_uri (NAUTILUS_WINDOW (window), uri);
}

static void
zoom_in_callback (NautilusZoomControl *zoom_control,
            NautilusWindow      *window)
{
	if (window->content_view != NULL) {
		nautilus_view_frame_zoom_in (window->content_view);
	}
}

static void
zoom_to_level_callback (NautilusZoomControl *zoom_control, gint level,
            		NautilusWindow      *window)
{
	if (window->content_view != NULL) {
		nautilus_view_frame_zoom_to_level (window->content_view, level);
	}
}

static void
zoom_out_callback (NautilusZoomControl *zoom_control,
                             NautilusWindow      *window)
{
	if (window->content_view != NULL) {
		nautilus_view_frame_zoom_out (window->content_view);
	}
}

static void
zoom_default_callback (NautilusZoomControl *zoom_control,
            	       NautilusWindow      *window)
{
	if (window->content_view != NULL) {
		nautilus_view_frame_zoom_default (window->content_view);
	}
}


static void
nautilus_window_constructed (NautilusWindow *window)
{
	GnomeApp *app;
	GtkWidget *location_bar_box, *statusbar;
  	GtkWidget *temp_frame;
  	GnomeDockItemBehavior behavior;
  	int sidebar_width;

  	app = GNOME_APP (window);

	/* set up location bar */
	location_bar_box = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_container_set_border_width (GTK_CONTAINER(location_bar_box), GNOME_PAD_SMALL);
	
	window->ent_uri = nautilus_location_bar_new ();
	
	gtk_signal_connect (GTK_OBJECT (window->ent_uri), "location_changed",
			    nautilus_window_goto_uri_callback, window);
	gtk_box_pack_start (GTK_BOX (location_bar_box), window->ent_uri,
			    TRUE, TRUE, GNOME_PAD_SMALL);
	behavior = GNOME_DOCK_ITEM_BEH_EXCLUSIVE
		| GNOME_DOCK_ITEM_BEH_NEVER_VERTICAL;
	if (!gnome_preferences_get_toolbar_detachable ()) {
		behavior |= GNOME_DOCK_ITEM_BEH_LOCKED;
	}

	gnome_app_add_docked (app, location_bar_box,
			      "uri-entry", behavior,
			      GNOME_DOCK_TOP, 2, 0, 0);

	/* Option menu for content view types; it's empty here, filled in when a uri is set. */
	window->view_as_option_menu = gtk_option_menu_new();
	gtk_box_pack_end (GTK_BOX (location_bar_box), window->view_as_option_menu, FALSE, FALSE, GNOME_PAD_SMALL);
	gtk_widget_show(window->view_as_option_menu);
	
	/* allocate the zoom control and place on the right next to the menu */
	window->zoom_control = nautilus_zoom_control_new ();
	gtk_widget_show (window->zoom_control);
	gtk_signal_connect (GTK_OBJECT (window->zoom_control), "zoom_in", zoom_in_callback, window);
	gtk_signal_connect (GTK_OBJECT (window->zoom_control), "zoom_out", zoom_out_callback, window);
	gtk_signal_connect (GTK_OBJECT (window->zoom_control), "zoom_to_level", zoom_to_level_callback, window);
	gtk_signal_connect (GTK_OBJECT (window->zoom_control), "zoom_default", zoom_default_callback, window);
	gtk_box_pack_end (GTK_BOX (location_bar_box), window->zoom_control, FALSE, FALSE, 0);
	
	gtk_widget_show_all (location_bar_box);
	
	/* set up status bar */
	statusbar = gtk_statusbar_new ();
	gnome_app_set_statusbar (app, statusbar);

	/* insert a little padding so text isn't jammed against frame */
	gtk_misc_set_padding (GTK_MISC (GTK_STATUSBAR (statusbar)->label), GNOME_PAD, 0);
	window->statusbar_ctx = gtk_statusbar_get_context_id (GTK_STATUSBAR (statusbar),
							      "IhateGtkStatusbar");
	
	/* FIXME bugzilla.eazel.com 1243: 
	 * We should use inheritance instead of these special cases
	 * for the desktop window.
	 */
        if (NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		window->content_hbox = gtk_widget_new (NAUTILUS_TYPE_GENEROUS_BIN, NULL);
	} else {
		/* set up window contents and policy */	
		gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, FALSE);

		/* FIXME bugzilla.eazel.com 1244: Hard-wired size here? */
		gtk_window_set_default_size (GTK_WINDOW (window), 650, 400);

		window->content_hbox = gtk_hpaned_new ();

		/* FIXME bugzilla.eazel.com 1245: No constant for the default? */
		/* FIXME bugzilla.eazel.com 1245: Saved in pixels instead of in %? */
		/* FIXME bugzilla.eazel.com 1245: No reality check on the value? */
		/* FIXME bugzilla.eazel.com 1245: get_enum? why not get_integer? */
		sidebar_width = nautilus_preferences_get_enum (NAUTILUS_PREFERENCES_SIDEBAR_WIDTH, 148);
		gtk_paned_set_position (GTK_PANED (window->content_hbox), sidebar_width);
	}
	
	gnome_app_set_contents (app, window->content_hbox);
	
	/* set up the index panel in a frame */
	temp_frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (temp_frame), GTK_SHADOW_OUT);
	gtk_widget_show (temp_frame);
	
	window->sidebar = nautilus_sidebar_new ();
	gtk_widget_show (GTK_WIDGET (window->sidebar));
	gtk_container_add (GTK_CONTAINER (temp_frame), GTK_WIDGET (window->sidebar));
	gtk_signal_connect (GTK_OBJECT (window->sidebar), "location_changed",
			    nautilus_window_goto_uri_callback, window);
	
	/* FIXME bugzilla.eazel.com 1243: 
	 * We should use inheritance instead of these special cases
	 * for the desktop window.
	 */
        if (!NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		gtk_paned_pack1 (GTK_PANED(window->content_hbox), temp_frame, FALSE, FALSE);
	}
	
	gtk_widget_show_all (window->content_hbox);
	
	/* enable mouse tracking for the index panel */
	/* FIXME bugzilla.eazel.com 1246: How about the sidebar doing this for itself. */
	gtk_widget_add_events (GTK_WIDGET (window->sidebar), GDK_POINTER_MOTION_MASK);

	/* CORBA and Bonobo setup */
	window->ui_handler = bonobo_ui_handler_new ();
	bonobo_ui_handler_set_app (window->ui_handler, app);
	bonobo_ui_handler_set_statusbar (window->ui_handler, statusbar);

	/* Create menus and toolbars */
	nautilus_window_initialize_menus (window);
	nautilus_window_initialize_toolbars (window);
	
	/* Set initial sensitivity of some buttons & menu items 
	 * now that they're all created.
	 */
	nautilus_window_allow_back (window, FALSE);
	nautilus_window_allow_forward (window, FALSE);
	nautilus_window_allow_stop (window, FALSE);

	/* Set up undo manager */
	nautilus_undo_manager_attach (window->application->undo_manager, GTK_OBJECT (window));
}

static void
nautilus_window_set_arg (GtkObject *object,
			 GtkArg *arg,
			 guint arg_id)
{
	GnomeApp *app = (GnomeApp *) object;
	char *old_app_name;
	NautilusWindow *window = (NautilusWindow *) object;
	
	switch(arg_id) {
	case ARG_APP_ID:
		if(!GTK_VALUE_STRING(*arg))
			return;
		
		old_app_name = app->name;
		g_free(app->name);
		app->name = g_strdup(GTK_VALUE_STRING(*arg));
		g_assert(app->name);
		g_free(app->prefix);
		app->prefix = g_strconcat("/", app->name, "/", NULL);
		if(!old_app_name) {
			nautilus_window_constructed(NAUTILUS_WINDOW(object));
		}
		break;
	case ARG_APP:
		window->application = NAUTILUS_APPLICATION (GTK_VALUE_OBJECT (*arg));
		break;
	case ARG_CONTENT_VIEW:
		nautilus_window_real_set_content_view (window, (NautilusViewFrame *)GTK_VALUE_OBJECT(*arg));
		break;
	}
}

static void
nautilus_window_get_arg (GtkObject *object,
			 GtkArg *arg,
			 guint arg_id)
{
	GnomeApp *app = (GnomeApp *) object;
	
	switch(arg_id) {
	case ARG_APP_ID:
		GTK_VALUE_STRING(*arg) = app->name;
		break;
	case ARG_APP:
		GTK_VALUE_OBJECT(*arg) = GTK_OBJECT(NAUTILUS_WINDOW(object)->application);
		break;
	case ARG_CONTENT_VIEW:
		GTK_VALUE_OBJECT(*arg) = GTK_OBJECT(((NautilusWindow *)object)->content_view);
		break;
	}
}

static void 
nautilus_window_destroy (NautilusWindow *window)
{
	NautilusWindowClass *klass = NAUTILUS_WINDOW_CLASS(GTK_OBJECT(window)->klass);
	
	/* Dont keep track of sidebar panel changes no more */
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_SIDEBAR_PANELS_NAMESPACE,
					      sidebar_panels_changed_callback,
					      NULL);
	
	nautilus_window_toolbar_remove_theme_callback();
	g_list_free (window->sidebar_panels);

	nautilus_view_identifier_free (window->content_view_id);
	
	g_free (window->location);
	nautilus_g_list_free_deep (window->selection);
	g_slist_foreach (window->back_list, (GFunc)gtk_object_unref, NULL);
	g_slist_foreach (window->forward_list, (GFunc)gtk_object_unref, NULL);
	g_slist_free (window->back_list);
	g_slist_free (window->forward_list);
	
	if (window->statusbar_clear_id != 0) {
		g_source_remove (window->statusbar_clear_id);
	}

	if (window->action_tag != 0) {
		g_source_remove (window->action_tag);
	}
	
	if (GTK_OBJECT_CLASS(klass->parent_class)->destroy)
		GTK_OBJECT_CLASS(klass->parent_class)->destroy(GTK_OBJECT(window));
}

void
nautilus_window_close (NautilusWindow *window)
{
        g_return_if_fail (NAUTILUS_IS_WINDOW (window));
	gtk_widget_destroy (GTK_WIDGET (window));
}


/* The reason for this function is that
   `gdk_pixbuf_render_pixmap_and_mask', as currently implemented, will
   fail on a pixbuf with no alpha channel. This function will instead
   return with NULL in *mask_retval in such a case. If that ever gets
   fixed, this function should be removed.
*/

static void
would_be_in_gdk_pixbuf_if_federico_wasnt_stubborn_pixbuf_render(GdkPixbuf  *pixbuf,
                                                                GdkPixmap **pixmap,
                                                                GdkBitmap **mask_retval,
                                                                gint        alpha_threshold)
{
        GdkBitmap *mask = NULL;
	
        g_return_if_fail(pixbuf != NULL);
	
        /* generate mask */
        if (gdk_pixbuf_get_has_alpha(pixbuf)) {
                mask = gdk_pixmap_new(NULL,
                                      gdk_pixbuf_get_width(pixbuf),
                                      gdk_pixbuf_get_height(pixbuf),
                                      1);

                gdk_pixbuf_render_threshold_alpha(pixbuf, mask,
                                                  0, 0, 0, 0,
                                                  gdk_pixbuf_get_width(pixbuf),
                                                  gdk_pixbuf_get_height(pixbuf),
                                                  alpha_threshold);
        }

        /* Draw to pixmap */

        if (pixmap != NULL) {
                GdkGC* gc;

                *pixmap = gdk_pixmap_new(NULL,
                                         gdk_pixbuf_get_width(pixbuf),
                                         gdk_pixbuf_get_height(pixbuf),
                                         gdk_rgb_get_visual()->depth);

                gc = gdk_gc_new(*pixmap);

                gdk_gc_set_clip_mask(gc, mask);

                gdk_pixbuf_render_to_drawable(pixbuf, *pixmap,
                                              gc,
                                              0, 0, 0, 0,
                                              gdk_pixbuf_get_width(pixbuf),
                                              gdk_pixbuf_get_height(pixbuf),
                                              GDK_RGB_DITHER_NORMAL,
                                              0, 0);

                gdk_gc_unref(gc);
        }

        if (mask_retval)
                *mask_retval = mask;
        else
                gdk_bitmap_unref(mask);
}

static void
nautilus_window_realize (GtkWidget *widget)
{
        GdkPixmap *pixmap = NULL;
        GdkBitmap *mask = NULL;
        gchar *filename;
        
        /* Create our GdkWindow */
        if (GTK_WIDGET_CLASS(parent_class)->realize)
                (* GTK_WIDGET_CLASS(parent_class)->realize) (widget);
        
        /* Set the mini icon */
        /* FIXME bugzilla.eazel.com 609:
         * Need a real icon for Nautilus here. It should be 16x16.
         */
        filename = gnome_pixmap_file("panel-arrow-down.png");
        
        if (filename != NULL) {
                GdkPixbuf *pixbuf;

                pixbuf = gdk_pixbuf_new_from_file(filename);

                if (pixbuf != NULL) {
                        would_be_in_gdk_pixbuf_if_federico_wasnt_stubborn_pixbuf_render(pixbuf,
                                                                                        &pixmap,
                                                                                        &mask,
                                                                                        128);
                }
        }
                
                
        if (pixmap != NULL)
                nautilus_set_mini_icon(widget->window,
                                       pixmap,
                                       mask);

        /* FIXME bugzilla.eazel.com 610:
         * I think we are leaking the pixmap/mask here.
         */
}

/*
 * Main API
 */

static void
nautilus_window_switch_views (NautilusWindow *window, NautilusViewIdentifier *id)
{
        NautilusDirectory *directory;
        NautilusViewFrame *view;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));
        g_return_if_fail (window->location != NULL);
	g_return_if_fail (id != NULL);

        directory = nautilus_directory_get (window->location);
        g_assert (directory != NULL);
        nautilus_mime_set_default_component_for_uri
		(window->location, id->iid);
        nautilus_directory_unref (directory);
        
        nautilus_window_allow_stop (window, TRUE);
        
        view = nautilus_window_load_content_view (window, id, NULL);
        nautilus_window_set_state_info (window,
                                        (NautilusWindowStateItem)NEW_CONTENT_VIEW_ACTIVATED, view,
                                        (NautilusWindowStateItem)0);	
}

static void
view_menu_switch_views_callback (GtkWidget *widget, gpointer data)
{
        NautilusWindow *window;
        NautilusViewIdentifier *identifier;
        
        g_return_if_fail (GTK_IS_MENU_ITEM (widget));
        g_return_if_fail (NAUTILUS_IS_WINDOW (gtk_object_get_data (GTK_OBJECT (widget), "window")));
        
        window = NAUTILUS_WINDOW (gtk_object_get_data (GTK_OBJECT (widget), "window"));
        identifier = (NautilusViewIdentifier *)gtk_object_get_data (GTK_OBJECT (widget), "identifier");
        
        nautilus_window_switch_views (window, identifier);
}

/* Note: The identifier parameter ownership is handed off to the menu item. */
static GtkWidget *
create_content_view_menu_item (NautilusWindow *window, NautilusViewIdentifier *identifier)
{
	GtkWidget *menu_item;
        char *menu_label;

	menu_label = g_strdup_printf (_("View as %s"), identifier->name);
	menu_item = gtk_menu_item_new_with_label (menu_label);
	g_free (menu_label);

	gtk_signal_connect
	        (GTK_OBJECT (menu_item),
	         "activate",
	         GTK_SIGNAL_FUNC (view_menu_switch_views_callback), 
	         NULL);

	/* Store copy of iid in item; free when item destroyed. */
	gtk_object_set_data_full (GTK_OBJECT (menu_item),
				  "identifier",
				  identifier,
				  (GtkDestroyNotify) nautilus_view_identifier_free);

	/* Store reference to window in item; no need to free this. */
	gtk_object_set_data (GTK_OBJECT (menu_item), "window", window);
	gtk_widget_show (menu_item);

	return menu_item;
}

/* Make a special first item in the "View as" option menu that represents
 * the current content view. This should only be called if the current
 * content view isn't already in the "View as" option menu.
 */
static void
replace_special_current_view_in_content_view_menu (NautilusWindow *window)
{
	GtkWidget *menu;
	GtkWidget *first_menu_item;
	GtkWidget *new_menu_item;
	
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (window->view_as_option_menu));

	/* Remove menu before changing contents so it is resized properly
	 * when reattached later in this function.
	 */
	gtk_widget_ref (menu);
	gtk_option_menu_remove_menu (GTK_OPTION_MENU (window->view_as_option_menu));

	first_menu_item = nautilus_gtk_container_get_first_child (GTK_CONTAINER (menu));
	g_assert (first_menu_item != NULL);

	if (GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (first_menu_item), "current content view"))) {
		gtk_container_remove (GTK_CONTAINER (menu), first_menu_item);
	} else {
		/* Prepend separator. */
		new_menu_item = gtk_menu_item_new ();
		gtk_widget_show (new_menu_item);
		gtk_menu_prepend (GTK_MENU (menu), new_menu_item);
	}

	new_menu_item = create_content_view_menu_item (window, nautilus_view_identifier_copy (window->content_view_id));
	gtk_object_set_data (GTK_OBJECT (new_menu_item), "current content view", GINT_TO_POINTER (TRUE));
	gtk_menu_prepend (GTK_MENU (menu), new_menu_item);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (window->view_as_option_menu), menu);
	gtk_widget_unref (menu);
}

/**
 * nautilus_window_synch_content_view_menu:
 * 
 * Set the visible item of the "View as" option menu to
 * match the current content view.
 * 
 * @window: The NautilusWindow whose "View as" option menu should be synched.
 */
void
nautilus_window_synch_content_view_menu (NautilusWindow *window)
{
	GList *children, *child;
	GtkWidget *menu;
	NautilusViewIdentifier *item_id;
	int index, matching_index;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (window->view_as_option_menu));
	if (menu == NULL) {
		return;
	}
	
	children = gtk_container_children (GTK_CONTAINER (menu));
	matching_index = -1;

	for (child = children, index = 0; child != NULL; child = child->next, ++index) {
		item_id = (NautilusViewIdentifier *)(gtk_object_get_data (GTK_OBJECT (child->data), "identifier"));
		if (item_id != NULL && strcmp (window->content_view->iid, item_id->iid) == 0) {
			matching_index = index;
			break;
		}
	}

	if (matching_index == -1) {
		replace_special_current_view_in_content_view_menu (window);
		matching_index = 0;
	}

	gtk_option_menu_set_history (GTK_OPTION_MENU (window->view_as_option_menu), 
				     matching_index);

	g_list_free (children);
}

static void
chose_component_callback (NautilusViewIdentifier *identifier, gpointer callback_data)
{
	/* Can't assume callback_data is a valid NautilusWindow, because
	 * it might be garbage if the program is exiting when this dialog
	 * is up.
	 */

	if (identifier != NULL) {
		g_return_if_fail (NAUTILUS_IS_WINDOW (callback_data));
		nautilus_window_switch_views (NAUTILUS_WINDOW (callback_data), identifier);
	}

	/* FIXME bugzilla.eazel.com 1334: 
	 * There should be some global way to signal that the
	 * file type associations have changed, so that the places that
	 * display these lists can react. For now, hardwire this case, which
	 * is the most obvious one by far.
	 */
	if (NAUTILUS_IS_WINDOW (callback_data)) {
		nautilus_window_load_content_view_menu (NAUTILUS_WINDOW (callback_data));
	}
}

static void
view_menu_choose_view_callback (GtkWidget *widget, gpointer data)
{
        NautilusWindow *window;
        NautilusFile *file;
        
        g_return_if_fail (GTK_IS_MENU_ITEM (widget));
        g_return_if_fail (NAUTILUS_IS_WINDOW (gtk_object_get_data (GTK_OBJECT (widget), "window")));
        
        window = NAUTILUS_WINDOW (gtk_object_get_data (GTK_OBJECT (widget), "window"));

	/* Set the option menu back to its previous setting (Don't leave it
	 * on this dialog-producing "View as Other..." setting). If the menu choice 
	 * causes a content view change, this will be updated again later, 
	 * in nautilus_window_load_content_view_menu. Do this right away so 
	 * the user never sees the option menu set to "View as Other...".
	 */
	nautilus_window_synch_content_view_menu (window);

        file = nautilus_file_get (window->location);
        g_return_if_fail (NAUTILUS_IS_FILE (file));

	nautilus_choose_component_for_file (file, 
					    GTK_WINDOW (window), 
					    chose_component_callback, 
					    window);

	nautilus_file_unref (file);
}

void
nautilus_window_load_content_view_menu (NautilusWindow *window)
{	
	GList *components;
        GList *p;
        GtkWidget *new_menu;
        int index;
        GtkWidget *menu_item;

        g_return_if_fail (NAUTILUS_IS_WINDOW (window));
        g_return_if_fail (GTK_IS_OPTION_MENU (window->view_as_option_menu));
        
        new_menu = gtk_menu_new ();
        
        /* Add a menu item for each view in the preferred list for this location. */
        components = nautilus_mime_get_short_list_components_for_uri (window->location);

        index = 0;
        for (p = components; p != NULL; p = p->next) {
                menu_item = create_content_view_menu_item 
                	(window, nautilus_view_identifier_new_from_content_view (p->data));
                gtk_menu_append (GTK_MENU (new_menu), menu_item);

                ++index;
        }

        /* Add "View as Other..." extra bonus choice, with separator before it.
         * Leave separator out if there are no viewers in menu by default. 
         */
        if (components != NULL) {
	        menu_item = gtk_menu_item_new ();
	        gtk_widget_show (menu_item);
	        gtk_menu_append (GTK_MENU (new_menu), menu_item);
        }

       	menu_item = gtk_menu_item_new_with_label (_("View as Other..."));
        /* Store reference to window in item; no need to free this. */
        gtk_object_set_data (GTK_OBJECT (menu_item), "window", window);
        gtk_signal_connect (GTK_OBJECT (menu_item),
        		    "activate",
        		    GTK_SIGNAL_FUNC (view_menu_choose_view_callback),
        		    NULL);
       	gtk_widget_show (menu_item);
       	gtk_menu_append (GTK_MENU (new_menu), menu_item);
        
        /*
         * We create and attach a new menu here because adding/removing
         * items from existing menu screws up the size of the option menu.
         */
        gtk_option_menu_set_menu (GTK_OPTION_MENU (window->view_as_option_menu),
                                  new_menu);

	nautilus_window_synch_content_view_menu (window);
}

void
nautilus_window_set_content_view (NautilusWindow *window, NautilusViewFrame *content_view)
{
	nautilus_window_real_set_content_view (window, content_view);
}

void
nautilus_window_add_sidebar_panel (NautilusWindow *window, NautilusViewFrame *sidebar_panel)
{
	g_return_if_fail (!g_list_find (window->sidebar_panels, sidebar_panel));
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (sidebar_panel));
	
	nautilus_sidebar_add_panel (window->sidebar, sidebar_panel);
	window->sidebar_panels = g_list_prepend (window->sidebar_panels, sidebar_panel);
}

void
nautilus_window_remove_sidebar_panel (NautilusWindow *window, NautilusViewFrame *sidebar_panel)
{
	if (!g_list_find(window->sidebar_panels, sidebar_panel))
		return;
	
	nautilus_sidebar_remove_panel (window->sidebar, sidebar_panel);
	window->sidebar_panels = g_list_remove (window->sidebar_panels, sidebar_panel);
}

void
nautilus_window_back_or_forward (NautilusWindow *window, gboolean back, guint distance)
{
	GSList *list;
	
	list = back ? window->back_list : window->forward_list;
	g_assert (g_slist_length (list) > distance);
	
	nautilus_window_begin_location_change
		(window,
		 nautilus_bookmark_get_uri (g_slist_nth_data (list, distance)),
		 NULL,
		 back ? NAUTILUS_LOCATION_CHANGE_BACK : NAUTILUS_LOCATION_CHANGE_FORWARD,
		 distance);
}

void
nautilus_window_go_back (NautilusWindow *window)
{
	nautilus_window_back_or_forward (window, TRUE, 0);
}

void
nautilus_window_go_forward (NautilusWindow *window)
{
	nautilus_window_back_or_forward (window, FALSE, 0);
}

void
nautilus_window_go_up (NautilusWindow *window)
{
	GnomeVFSURI *current_uri;
	GnomeVFSURI *parent_uri;
	char *parent_uri_string;
	
	if (window->location == NULL)
		return;
	
	current_uri = gnome_vfs_uri_new (window->location);
	parent_uri = gnome_vfs_uri_get_parent (current_uri);
	gnome_vfs_uri_unref (current_uri);
	parent_uri_string = gnome_vfs_uri_to_string (parent_uri, GNOME_VFS_URI_HIDE_NONE);
	gnome_vfs_uri_unref (parent_uri);  
	
	nautilus_window_goto_uri (window, parent_uri_string);
	
	g_free (parent_uri_string);
}

void
nautilus_window_go_home (NautilusWindow *window)
{
	char *default_home_uri, *home_uri;

	default_home_uri = g_strdup_printf ("file://%s", g_get_home_dir());
	home_uri = nautilus_preferences_get (NAUTILUS_PREFERENCES_HOME_URI, default_home_uri);
	g_free (default_home_uri);
	
	g_assert (home_uri != NULL);
	nautilus_window_goto_uri (window, home_uri);
	g_free (home_uri);
}

void
nautilus_window_allow_back (NautilusWindow *window, gboolean allow)
{
	gtk_widget_set_sensitive(window->back_button, allow); 
	bonobo_ui_handler_menu_set_sensitivity(window->ui_handler, NAUTILUS_MENU_PATH_BACK_ITEM, allow);
}

void
nautilus_window_allow_forward (NautilusWindow *window, gboolean allow)
{
	gtk_widget_set_sensitive(window->forward_button, allow); 
	bonobo_ui_handler_menu_set_sensitivity(window->ui_handler, NAUTILUS_MENU_PATH_FORWARD_ITEM, allow);
}

void
nautilus_window_allow_up (NautilusWindow *window, gboolean allow)
{
	gtk_widget_set_sensitive(window->up_button, allow); 
	bonobo_ui_handler_menu_set_sensitivity(window->ui_handler, NAUTILUS_MENU_PATH_UP_ITEM, allow);
}

void
nautilus_window_allow_reload (NautilusWindow *window, gboolean allow)
{
	gtk_widget_set_sensitive(window->reload_button, allow); 
}

void
nautilus_window_allow_stop (NautilusWindow *window, gboolean allow)
{
	gtk_widget_set_sensitive (window->stop_button, allow);
}

void
nautilus_send_history_list_changed (void)
{
	gtk_signal_emit_by_name (GTK_OBJECT (nautilus_signaller_get_current ()),
			 	 "history_list_changed");
}

void
nautilus_add_to_history_list (NautilusBookmark *bookmark)
{
	/* Note that the history is shared amongst all windows so
	 * this is not a NautilusWindow function. Perhaps it belongs
	 * in its own file.
	 */
	GSList *found_link;

	g_return_if_fail (NAUTILUS_IS_BOOKMARK (bookmark));

	found_link = g_slist_find_custom (history_list, 
					  bookmark,
					  nautilus_bookmark_compare_with);
	
	/* Remove any older entry for this same item. There can be at most 1. */
	if (found_link != NULL)
	{
		gtk_object_unref (found_link->data);
		history_list = g_slist_remove_link (history_list, found_link);
	}

	/* New item goes first. */
	gtk_object_ref (GTK_OBJECT (bookmark));
	history_list = g_slist_prepend(history_list, bookmark);

	/* Tell world that history list has changed. At least all the
	 * NautilusWindows (not just this one) are listening.
	 */
	nautilus_send_history_list_changed ();
}

GSList *
nautilus_get_history_list (void)
{
	return history_list;
}

static void
nautilus_window_open_location_callback (NautilusViewFrame *view,
					const char *location,
					NautilusWindow *window)
{
	nautilus_window_open_location (window, location, view);
}

static void
nautilus_window_open_location_in_new_window_callback (NautilusViewFrame *view,
						      const char *location,
						      NautilusWindow *window)
{
	nautilus_window_open_location_in_new_window (window, location, view);
}

static void
nautilus_window_report_location_change_callback (NautilusViewFrame *view,
						 const char *location,
						 NautilusWindow *window)
{
	nautilus_window_report_location_change (window, location, view);
}

static void
nautilus_window_report_selection_change_callback (NautilusViewFrame *view,
						  GList *selection,
						  NautilusWindow *window)
{
	nautilus_window_report_selection_change (window, selection, view);
}

static void
nautilus_window_report_status_callback (NautilusViewFrame *view,
					const char *status,
					NautilusWindow *window)
{
	nautilus_window_report_status (window, status, view);
}

static void
nautilus_window_report_load_underway_callback (NautilusViewFrame *view,
					       NautilusWindow *window)
{
	nautilus_window_report_load_underway (window, view);
}

static void
nautilus_window_report_load_progress_callback (NautilusViewFrame *view,
					       double fraction_done,
					       NautilusWindow *window)
{
	nautilus_window_report_load_progress (window, fraction_done, view);
}

static void
nautilus_window_report_load_complete_callback (NautilusViewFrame *view,
					       NautilusWindow *window)
{
	nautilus_window_report_load_complete (window, view);
}

static void
nautilus_window_report_load_failed_callback (NautilusViewFrame *view,
					     NautilusWindow *window)
{
	nautilus_window_report_load_failed (window, view);
}

static void
nautilus_window_set_title_callback (NautilusViewFrame *view, 
				    const char *title,
				    NautilusWindow *window)
{
	nautilus_window_set_title (window, title, view);
}

void
nautilus_window_connect_view (NautilusWindow *window, NautilusViewFrame *view)
{
	GtkObject *view_object;
	
	view_object = GTK_OBJECT (view);

	#define CONNECT(signal) gtk_signal_connect (view_object, #signal, nautilus_window_##signal##_callback, window)

	CONNECT (open_location);
	CONNECT (open_location_in_new_window);
	CONNECT (report_location_change);
	CONNECT (report_selection_change);
	CONNECT (report_status);
	CONNECT (report_load_underway);
	CONNECT (report_load_progress);
	CONNECT (report_load_complete);
	CONNECT (report_load_failed);
	CONNECT (set_title);

	#undef CONNECT

	gtk_signal_connect_object
		(view_object,
		 "destroy",
		 nautilus_window_view_destroyed,
		 GTK_OBJECT (window));
	gtk_signal_connect_object
		(view_object,
		 "client_gone",
		 nautilus_window_view_destroyed,
		 GTK_OBJECT (window));
}

void
nautilus_window_connect_content_view (NautilusWindow *window, NautilusViewFrame *view)
{
	nautilus_window_connect_view (window, NAUTILUS_VIEW_FRAME (view));
}

void
nautilus_window_display_error(NautilusWindow *window, const char *error_msg)
{
	GtkWidget *dialog;
	
	dialog = gnome_message_box_new (error_msg, GNOME_MESSAGE_BOX_ERROR, _("Close"), NULL);
	gnome_dialog_set_close (GNOME_DIALOG(dialog), TRUE);
	
	gnome_dialog_set_default (GNOME_DIALOG(dialog), 0);
	
	gtk_widget_show (dialog);
}

static void
nautilus_window_real_set_content_view (NautilusWindow *window, NautilusViewFrame *new_view)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));
	g_return_if_fail (new_view == NULL || NAUTILUS_IS_VIEW_FRAME (new_view));
	
	if (new_view == window->content_view) {
		return;
	}
	
	if (window->content_view != NULL) {
		gtk_container_remove (GTK_CONTAINER (window->content_hbox),
				      GTK_WIDGET (window->content_view));      
	}
	
	if (new_view != NULL) {
		nautilus_zoom_control_reset_zoom_level
			(NAUTILUS_ZOOM_CONTROL (window->zoom_control));
		
		gtk_widget_show (GTK_WIDGET (new_view));
		
		nautilus_view_frame_activate (new_view); 
		
		/* FIXME bugzilla.eazel.com 1243: 
		 * We should use inheritance instead of these special cases
		 * for the desktop window.
		 */
		if (!GTK_IS_PANED (window->content_hbox)) {
			gtk_container_add (GTK_CONTAINER (window->content_hbox),
					   GTK_WIDGET (new_view));
		} else {
			gtk_paned_pack2 (GTK_PANED (window->content_hbox),
					 GTK_WIDGET (new_view),
					 TRUE, FALSE);
		}
	}

	window->content_view = new_view;
}

/* reload the contents of the window */
void
nautilus_window_reload (NautilusWindow *window)
{
	nautilus_window_begin_location_change
		(window, window->location, NULL,
		 NAUTILUS_LOCATION_CHANGE_RELOAD, 0);
}

/**
 * window_find_sidebar_panel_by_identifier:
 * @window:	A NautilusWindow
 * @identifier: The NautilusViewIdentifier to look for
 *
 * Search the list of sidebar panels in the given window for one that
 * matches the given view identifier.
 *
 * Returns a referenced object, not a floating one. bonobo_object_unref
 * it when done playing with it.
 */
static NautilusViewFrame *
window_find_sidebar_panel_by_identifier (NautilusWindow *window, NautilusViewIdentifier *identifier)
{
        GList *iterator;

	g_assert (window != NULL);
	g_assert (NAUTILUS_IS_WINDOW (window));
	g_assert (identifier != NULL);

        for (iterator = window->sidebar_panels; iterator != NULL; iterator = iterator->next) {
		NautilusViewFrame *sidebar_panel;
		
		g_assert (iterator->data != NULL);
		g_assert (NAUTILUS_IS_VIEW_FRAME (iterator->data));
		
		sidebar_panel = NAUTILUS_VIEW_FRAME (iterator->data);
		
		if (strcmp (sidebar_panel->iid, identifier->iid) == 0) {
			gtk_widget_ref (GTK_WIDGET (sidebar_panel));
			return sidebar_panel;
		}
        }
	
	return NULL;
}

/**
 * window_update_sidebar_panels_from_preferences:
 * @window:	A NautilusWindow
 *
 * Update the current list of sidebar panels from preferences.   
 *
 * Disabled panels are removed if they are already in the list.
 *
 * Enabled panels are added if they are not already in the list.
 *
 */
static void
window_update_sidebar_panels_from_preferences (NautilusWindow *window)
{
	GList *enabled_view_identifier_list = NULL;
	GList *disabled_view_identifier_list = NULL;
	GList *iterator = NULL;

	g_assert (window != NULL);
	g_assert (NAUTILUS_IS_WINDOW (window));

	/* Obtain list of disabled view identifiers */
	disabled_view_identifier_list = 
		nautilus_global_preferences_get_disabled_sidebar_panel_view_identifiers ();

	/* Remove disabled panels from the window as needed */
	for (iterator = disabled_view_identifier_list; iterator != NULL; iterator = iterator->next) {
		NautilusViewIdentifier *identifier;
		NautilusViewFrame *sidebar_panel;
		
		g_assert (iterator->data != NULL);
		
		identifier = (NautilusViewIdentifier *) iterator->data;
		
		sidebar_panel = window_find_sidebar_panel_by_identifier (window, identifier);

		if (sidebar_panel != NULL) {
			nautilus_window_remove_sidebar_panel (window, sidebar_panel);

			gtk_widget_unref (GTK_WIDGET (sidebar_panel));
		}
	}

	if (disabled_view_identifier_list) {
		nautilus_view_identifier_list_free (disabled_view_identifier_list);
	}

	/* Obtain list of enabled view identifiers */
	enabled_view_identifier_list = 
		nautilus_global_preferences_get_enabled_sidebar_panel_view_identifiers ();
	
	/* Add enabled panels from the window as needed */
	for (iterator = enabled_view_identifier_list; iterator != NULL; iterator = iterator->next) {
		NautilusViewIdentifier *identifier;
		NautilusViewFrame *sidebar_panel;

		g_assert (iterator->data != NULL);
		
		identifier = (NautilusViewIdentifier *) iterator->data;

		sidebar_panel = window_find_sidebar_panel_by_identifier (window, identifier);

		if (sidebar_panel == NULL) {
			gboolean load_result;

			sidebar_panel = nautilus_view_frame_new (window->ui_handler,
								 window->application->undo_manager);
			nautilus_window_connect_view (window, sidebar_panel);
			
			load_result = nautilus_view_frame_load_client (sidebar_panel, identifier->iid);
			
			/* Make sure the load_client succeeded */
			if (load_result) {
				gtk_object_ref (GTK_OBJECT (sidebar_panel));
				
				nautilus_view_frame_set_active_errors (sidebar_panel, TRUE);
				
				nautilus_view_frame_set_label (sidebar_panel, identifier->name);
				
				nautilus_window_add_sidebar_panel (window, sidebar_panel);
			}
			else {
				g_warning ("sidebar_panels_changed_callback: Failed to load_client for '%s' meta view.", 
					   identifier->iid);
				
				gtk_widget_unref (GTK_WIDGET (sidebar_panel));
				
				sidebar_panel = NULL;
			}
		}
		else {
			gtk_widget_unref (GTK_WIDGET (sidebar_panel));
		}
	}

	nautilus_view_identifier_list_free (enabled_view_identifier_list);
}

/**
 * sidebar_panels_changed_callback:
 * @user_data:	Callback data
 *
 * Called when enabled/disabled preferences change for any
 * sidebar panel.
 */
static void
sidebar_panels_changed_callback (gpointer user_data)
{
	window_update_sidebar_panels_from_preferences (NAUTILUS_WINDOW (user_data));
}
