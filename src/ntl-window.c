/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

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

/* ntl-window.c: Implementation of the main window object */

#include <config.h>
#include "ntl-window-private.h"

#include "ntl-window-msgs.h"
#include "ntl-window-state.h"
#include "ntl-app.h"
#include "ntl-meta-view.h"

#include <gnome.h>
#include <math.h>
#include "nautilus-bookmarks-window.h"
#include "nautilus-signaller.h"
#include "nautilus-location-bar.h"
#include "ntl-index-panel.h"
#include "ntl-miniicon.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libnautilus/nautilus-bonobo-ui.h>

#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-program-choosing.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-view-identifier.h>
#include <libnautilus/nautilus-undo-manager.h>
#include "nautilus-zoom-control.h"
#include <ctype.h>
#include <libgnomevfs/gnome-vfs-uri.h>

enum
{
	LAST_SIGNAL
};

static void nautilus_window_realize (GtkWidget *widget);
static void nautilus_window_real_set_content_view (NautilusWindow *window, NautilusViewFrame *new_view);

/* Object framework static variables */
static GnomeAppClass *parent_class = NULL;
static guint window_signals[LAST_SIGNAL];

/* Other static variables */
static GSList *history_list = NULL;


/* Stuff for handling the CORBA interface */
typedef struct {
  POA_Nautilus_ViewWindow servant;
  gpointer bonobo_object;

  NautilusWindow *window;
} impl_POA_Nautilus_ViewWindow;

static const CORBA_char *impl_Nautilus_ViewWindow__get_current_uri (impl_POA_Nautilus_ViewWindow *servant,
                                                                    CORBA_Environment            *ev);

static Nautilus_Application impl_Nautilus_ViewWindow__get_application (impl_POA_Nautilus_ViewWindow *servant,
                                                                       CORBA_Environment            *ev);

static void impl_Nautilus_ViewWindow_open_uri                         (impl_POA_Nautilus_ViewWindow *servant,
                                                                       CORBA_char                   *uri,
                                                                       CORBA_Environment            *ev);

static void impl_Nautilus_ViewWindow_close                            (impl_POA_Nautilus_ViewWindow *servant,
                                                                       CORBA_Environment            *ev);


static POA_Nautilus_ViewWindow__epv impl_Nautilus_ViewWindow_epv =
{
  NULL,
  (gpointer) &impl_Nautilus_ViewWindow__get_current_uri,
  (gpointer) &impl_Nautilus_ViewWindow__get_application,
  (gpointer) &impl_Nautilus_ViewWindow_open_uri,
  (gpointer) &impl_Nautilus_ViewWindow_close
};

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

static POA_Nautilus_ViewWindow__vepv impl_Nautilus_ViewWindow_vepv =
{
   &base_epv,
   NULL,
   &impl_Nautilus_ViewWindow_epv
};

static const CORBA_char *
impl_Nautilus_ViewWindow__get_current_uri (impl_POA_Nautilus_ViewWindow *servant,
                                           CORBA_Environment            *ev)
{
  return nautilus_window_get_requested_uri (servant->window);
}


static Nautilus_Application
impl_Nautilus_ViewWindow__get_application (impl_POA_Nautilus_ViewWindow *servant,
                                           CORBA_Environment            *ev)
{
  return CORBA_Object_duplicate(bonobo_object_corba_objref(servant->window->app), ev);
}

static void
impl_Nautilus_ViewWindow_open_uri (impl_POA_Nautilus_ViewWindow *servant,
                                   CORBA_char                   *uri,
                                   CORBA_Environment            *ev)
{
  nautilus_window_goto_uri (servant->window, uri);
}

static void
impl_Nautilus_ViewWindow_close (impl_POA_Nautilus_ViewWindow *servant,
                                CORBA_Environment            *ev)
{
  nautilus_window_close (servant->window);
}


static void
impl_Nautilus_ViewWindow__destroy(BonoboObject *obj, impl_POA_Nautilus_ViewWindow *servant)
{
   PortableServer_ObjectId *objid;
   CORBA_Environment ev;

   CORBA_exception_init(&ev);

   objid = PortableServer_POA_servant_to_id(bonobo_poa(), servant, &ev);
   PortableServer_POA_deactivate_object(bonobo_poa(), objid, &ev);
   CORBA_free(objid);
   obj->servant = NULL;

   POA_Nautilus_ViewWindow__fini((PortableServer_Servant) servant, &ev);
   g_free(servant);

   CORBA_exception_free(&ev);
}

static BonoboObject *
impl_Nautilus_ViewWindow__create(NautilusWindow *window)
{
   BonoboObject *retval;
   impl_POA_Nautilus_ViewWindow *newservant;
   CORBA_Environment ev;

   CORBA_exception_init(&ev);

   newservant = g_new0(impl_POA_Nautilus_ViewWindow, 1);
   newservant->servant.vepv = &impl_Nautilus_ViewWindow_vepv;
   newservant->window = window;
   POA_Nautilus_ViewWindow__init((PortableServer_Servant) newservant, &ev);

   retval = bonobo_object_new_from_servant(newservant);

   gtk_signal_connect(GTK_OBJECT(retval), "destroy", GTK_SIGNAL_FUNC(impl_Nautilus_ViewWindow__destroy), newservant);
   CORBA_exception_free(&ev);

   return retval;
}

static void nautilus_window_class_init (NautilusWindowClass *klass);
static void nautilus_window_init (NautilusWindow *window);
static void nautilus_window_destroy (NautilusWindow *window);
static void nautilus_window_set_arg (GtkObject      *object,
                                     GtkArg         *arg,
                                     guint	      arg_id);
static void nautilus_window_get_arg (GtkObject      *object,
                                     GtkArg         *arg,
                                     guint	      arg_id);
static void nautilus_window_goto_uri_cb (GtkWidget *widget,
                                         const char *uri,
                                         GtkWidget *window);
static void zoom_in_cb  (NautilusZoomControl *zoom_control,
                         NautilusWindow      *window);
static void zoom_out_cb (NautilusZoomControl *zoom_control,
                         NautilusWindow      *window);

/* milliseconds */
#define STATUSBAR_CLEAR_TIMEOUT 5000
	
GtkType
nautilus_window_get_type(void)
{
  static guint window_type = 0;

  if (!window_type)
    {
      GtkTypeInfo window_info =
      {
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

#if 0
typedef void (*GtkSignal_NONE__BOXED_OBJECT) (GtkObject * object,
                                              gpointer arg1,
                                              GtkObject *arg2,
					       gpointer user_data);
static void 
gtk_marshal_NONE__BOXED_OBJECT (GtkObject * object,
			       GtkSignalFunc func,
			       gpointer func_data,
			       GtkArg * args)
{
  GtkSignal_NONE__BOXED_OBJECT rfunc;
  rfunc = (GtkSignal_NONE__BOXED_OBJECT) func;
  (*rfunc) (object,
	    GTK_VALUE_BOXED (args[0]),
	    GTK_VALUE_OBJECT (args[1]),
	    func_data);
}
#endif

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

  gtk_object_class_add_signals (object_class, window_signals, LAST_SIGNAL);

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
  impl_Nautilus_ViewWindow_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv();

  widget_class->realize = nautilus_window_realize;
}

static void
nautilus_window_init (NautilusWindow *window)
{
  gtk_quit_add_destroy (1, GTK_OBJECT (window));
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
  if(txt && *txt)
    {
      window->statusbar_clear_id = g_timeout_add(STATUSBAR_CLEAR_TIMEOUT, (GSourceFunc)nautilus_window_clear_status, window);
      gtk_statusbar_push(GTK_STATUSBAR(GNOME_APP(window)->statusbar), window->statusbar_ctx, txt);
    }
  else
    window->statusbar_clear_id = 0;
}

void
nautilus_window_goto_uri (NautilusWindow *window, const char *uri)
{
  Nautilus_NavigationRequestInfo navinfo;

  memset(&navinfo, 0, sizeof(navinfo));
  navinfo.requested_uri = (char *)uri;
  navinfo.new_window_requested = FALSE;

  nautilus_window_request_location_change (window, &navinfo, NULL);
}

static void
nautilus_window_goto_uri_cb (GtkWidget *widget,
                             const char *uri,
                             GtkWidget *window)
{
  nautilus_window_goto_uri(NAUTILUS_WINDOW(window), uri);
}

static void
zoom_in_cb (NautilusZoomControl *zoom_control,
            NautilusWindow      *window)
{
  if (window->content_view != NULL) {
    nautilus_view_frame_zoom_in (window->content_view);
  }
}

static void
zoom_out_cb (NautilusZoomControl *zoom_control,
                             NautilusWindow      *window)
{
  if (window->content_view != NULL) {
    nautilus_view_frame_zoom_out (window->content_view);
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
	NautilusUndoManager *app_undo_manager;
  	Nautilus_Undo_Manager undo_manager;
	CORBA_Environment ev;

  	CORBA_exception_init(&ev);

  
  	app = GNOME_APP(window);

	/* Set up undo manager */
	app_undo_manager = NAUTILUS_UNDO_MANAGER (NAUTILUS_APP (window->app)->undo_manager);	
	g_assert (app_undo_manager != NULL);
	undo_manager = bonobo_object_corba_objref (BONOBO_OBJECT (app_undo_manager));
	Bonobo_Unknown_ref (undo_manager, &ev);
	nautilus_attach_undo_manager (GTK_OBJECT (window), undo_manager);

	/* set up location bar */
	location_bar_box = gtk_hbox_new(FALSE, GNOME_PAD);
	gtk_container_set_border_width(GTK_CONTAINER(location_bar_box), GNOME_PAD_SMALL);

	window->ent_uri = nautilus_location_bar_new();	
	nautilus_location_bar_enable_undo (NAUTILUS_LOCATION_BAR (window->ent_uri), TRUE);
	
	gtk_signal_connect(GTK_OBJECT(window->ent_uri), "location_changed",
                     nautilus_window_goto_uri_cb, window);
	gtk_box_pack_start(GTK_BOX(location_bar_box), window->ent_uri, TRUE, TRUE, GNOME_PAD_SMALL);
  	behavior = GNOME_DOCK_ITEM_BEH_EXCLUSIVE | GNOME_DOCK_ITEM_BEH_NEVER_VERTICAL;
	if(!gnome_preferences_get_toolbar_detachable())
		behavior |= GNOME_DOCK_ITEM_BEH_LOCKED;
	gnome_app_add_docked(app, location_bar_box, "uri-entry", behavior, GNOME_DOCK_TOP, 2, 0, 0);

	/* Option menu for content view types; it's empty here, filled in when a uri is set. */
	window->view_as_option_menu = gtk_option_menu_new();
	gtk_box_pack_end(GTK_BOX(location_bar_box), window->view_as_option_menu, FALSE, FALSE, GNOME_PAD_SMALL);
	gtk_widget_show(window->view_as_option_menu);

	/* allocate the zoom control and place on the right next to the menu */
	window->zoom_control = nautilus_zoom_control_new ();
	gtk_widget_show (window->zoom_control);
	gtk_signal_connect (GTK_OBJECT (window->zoom_control), "zoom_in", zoom_in_cb, window);
	gtk_signal_connect (GTK_OBJECT (window->zoom_control), "zoom_out", zoom_out_cb, window);
	gtk_box_pack_end (GTK_BOX (location_bar_box), window->zoom_control, FALSE, FALSE, 0);
  
	gtk_widget_show_all(location_bar_box);

	/* set up status bar */
	gnome_app_set_statusbar(app, (statusbar = gtk_statusbar_new()));
	
	/* insert a little padding so text isn't jammed against frame */
	gtk_misc_set_padding(GTK_MISC ((GTK_STATUSBAR (statusbar))->label), GNOME_PAD, 0);
	window->statusbar_ctx = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar), "IhateGtkStatusbar");

	/* set up window contents and policy */	
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);
	gtk_window_set_default_size(GTK_WINDOW(window), 650, 400);

	window->content_hbox = gtk_hpaned_new();
	sidebar_width = nautilus_preferences_get_enum(NAUTILUS_PREFERENCES_SIDEBAR_WIDTH, 148);
	gtk_paned_set_position(GTK_PANED(window->content_hbox), sidebar_width);
 
	gnome_app_set_contents(app, window->content_hbox);

	/* set up the index panel in a frame */
	temp_frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(temp_frame), GTK_SHADOW_OUT);
	gtk_widget_show(temp_frame);
  
	window->index_panel = nautilus_index_panel_new();
	gtk_widget_show (GTK_WIDGET (window->index_panel));
	gtk_container_add(GTK_CONTAINER(temp_frame), GTK_WIDGET (window->index_panel));

	gtk_paned_pack1(GTK_PANED(window->content_hbox), temp_frame, FALSE, FALSE);

	gtk_widget_show_all(window->content_hbox);

	/* enable mouse tracking for the index panel */
	gtk_widget_add_events(GTK_WIDGET (window->index_panel), GDK_POINTER_MOTION_MASK);

	/* CORBA and Bonobo setup */
	window->ntl_viewwindow = impl_Nautilus_ViewWindow__create(window);
	window->uih = bonobo_ui_handler_new();
	bonobo_ui_handler_set_app(window->uih, app);
	bonobo_ui_handler_set_statusbar(window->uih, statusbar);

	/* Create menus and toolbars */
	nautilus_window_initialize_menus (window);
	nautilus_window_initialize_toolbars (window);
	
	/* Set initial sensitivity of some buttons & menu items 
	* now that they're all created.
	*/
	nautilus_window_allow_back(window, FALSE);
	nautilus_window_allow_forward(window, FALSE);
	nautilus_window_allow_stop(window, FALSE);

	CORBA_exception_free(&ev);
}

static void
nautilus_window_set_arg (GtkObject      *object,
			  GtkArg         *arg,
			  guint	      arg_id)
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
    	window->app = BONOBO_OBJECT(GTK_VALUE_OBJECT(*arg));
    break;
  case ARG_CONTENT_VIEW:
    nautilus_window_real_set_content_view (window, (NautilusViewFrame *)GTK_VALUE_OBJECT(*arg));
    break;
  }
}

static void
nautilus_window_get_arg (GtkObject      *object,
			  GtkArg         *arg,
			  guint	      arg_id)
{
  GnomeApp *app = (GnomeApp *) object;

  switch(arg_id) {
  case ARG_APP_ID:
    GTK_VALUE_STRING(*arg) = app->name;
    break;
  case ARG_APP:
    GTK_VALUE_OBJECT(*arg) = GTK_OBJECT(NAUTILUS_WINDOW(object)->app);
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

  g_list_free (window->meta_views);

  CORBA_free(window->ni);
  CORBA_free(window->si);
  g_slist_foreach(window->back_list, (GFunc)gtk_object_unref, NULL);
  g_slist_foreach(window->forward_list, (GFunc)gtk_object_unref, NULL);
  g_slist_free(window->back_list);
  g_slist_free(window->forward_list);

  if(window->statusbar_clear_id)
    g_source_remove(window->statusbar_clear_id);

  if(GTK_OBJECT_CLASS(klass->parent_class)->destroy)
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

#if 0
static gboolean
nautilus_window_send_show_properties(GtkWidget *dockitem, GdkEventButton *event, NautilusViewFrame *meta_view)
{
  if(event->button != 3)
    return FALSE;

  gtk_signal_emit_stop_by_name(GTK_OBJECT(dockitem), "button_press_event");

  nautilus_view_show_properties(meta_view);

  return TRUE;
}
#endif

static void
nautilus_window_switch_views (NautilusWindow *window, const char *iid)
{
        NautilusDirectory *directory;
        NautilusViewFrame *view;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));
        g_return_if_fail (NAUTILUS_WINDOW (window)->ni != NULL);
	g_return_if_fail (iid != NULL);

        directory = nautilus_directory_get (window->ni->requested_uri);
        g_assert (directory != NULL);
        nautilus_directory_set_metadata (directory,
                                         NAUTILUS_METADATA_KEY_INITIAL_VIEW,
                                         NULL,
                                         iid);
        nautilus_directory_unref (directory);
        
        nautilus_window_allow_stop (window, TRUE);
        
        view = nautilus_window_load_content_view (window, iid, window->ni, NULL);
        nautilus_window_set_state_info (window,
                                        (NautilusWindowStateItem)NEW_CONTENT_VIEW_ACTIVATED, view,
                                        (NautilusWindowStateItem)0);	
}

/**
 * synch_view_as_menu:
 * 
 * Set the visible item of the "View as" option menu to
 * match the current content view.
 * 
 * @window: The NautilusWindow whose "View as" option menu should be synched.
 */
static void
synch_view_as_menu (NautilusWindow *window)
{
	GList *children, *child;
	GtkWidget *menu;
	const char *item_iid;
	int index, matching_index;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (window->view_as_option_menu));
	if (menu == NULL) {
		return;
	}
	children = gtk_container_children (GTK_CONTAINER (menu));
	matching_index = -1;

	for (child = children, index = 0; child != NULL; child = child->next, ++index) {
		item_iid = (const char *)(gtk_object_get_data (GTK_OBJECT (child->data), "iid"));
		if (nautilus_strcmp (window->content_view->iid, item_iid) == 0) {
			matching_index = index;
			break;
		}
	}

	if (matching_index != -1) {
		gtk_option_menu_set_history (GTK_OPTION_MENU (window->view_as_option_menu), 
					     matching_index);
	} else {
		g_warning ("In synch_view_as_menu, couldn't find matching menu item.");
	}

	g_list_free (children);
}

static void
chose_component_callback (NautilusViewIdentifier *identifier, gpointer callback_data)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (callback_data));

	if (identifier != NULL) {
		/* FIXME: Need to add menu item for new identifier to "View as" menu. */
		nautilus_window_switch_views (NAUTILUS_WINDOW (callback_data), identifier->iid);
	}

	nautilus_view_identifier_free (identifier);
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
	 * on this dialog-producing "View as ..." setting). If the menu choice 
	 * causes a content view change, this will be updated again later, 
	 * in nautilus_window_load_content_view_menu. Do this right away so 
	 * the user never sees the option menu set to "View as ...".
	 */
	synch_view_as_menu (window);

        file = nautilus_file_get (window->ni->requested_uri);
        g_return_if_fail (NAUTILUS_IS_FILE (file));

	nautilus_choose_component_for_file (file, 
					    GTK_WINDOW (window), 
					    chose_component_callback, 
					    window);

	nautilus_file_unref (file);
}

static void
view_menu_switch_views_callback (GtkWidget *widget, gpointer data)
{
        NautilusWindow *window;
        const char *iid;
        
        g_return_if_fail (GTK_IS_MENU_ITEM (widget));
        g_return_if_fail (NAUTILUS_IS_WINDOW (gtk_object_get_data (GTK_OBJECT (widget), "window")));
        
        window = NAUTILUS_WINDOW (gtk_object_get_data (GTK_OBJECT (widget), "window"));
        iid = (const char *)gtk_object_get_data (GTK_OBJECT (widget), "iid");
        
        nautilus_window_switch_views (window, iid);        
}

void
nautilus_window_load_content_view_menu (NautilusWindow *window,
                                        NautilusNavigationInfo *ni)
{
        GList *p;
        GtkWidget *new_menu;
        int index;
        GtkWidget *menu_item;
        NautilusViewIdentifier *identifier;
        char *menu_label;

        g_return_if_fail (NAUTILUS_IS_WINDOW (window));
        g_return_if_fail (GTK_IS_OPTION_MENU (window->view_as_option_menu));
        g_return_if_fail (ni != NULL);
        
        new_menu = gtk_menu_new ();
        
        /* Add a menu item for each available content view type */
        index = 0;
        for (p = ni->content_identifiers; p != NULL; p = p->next) {
                identifier = (NautilusViewIdentifier *) p->data;
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
					  "iid",
					  g_strdup (identifier->iid),
					  g_free);

                /* Store reference to window in item; no need to free this. */
                gtk_object_set_data (GTK_OBJECT (menu_item), "window", window);
                gtk_menu_append (GTK_MENU (new_menu), menu_item);
                gtk_widget_show (menu_item);

                ++index;
        }

        /* Add "View as ..." extra bonus choice, with separator before it. */
        menu_item = gtk_menu_item_new ();
        gtk_widget_show (menu_item);
        gtk_menu_append (GTK_MENU (new_menu), menu_item);

       	menu_item = gtk_menu_item_new_with_label (_("View as ..."));
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

	synch_view_as_menu (window);
}

void
nautilus_window_set_content_view (NautilusWindow *window, NautilusViewFrame *content_view)
{
  nautilus_window_real_set_content_view (window, content_view);
}

void
nautilus_window_add_meta_view(NautilusWindow *window, NautilusViewFrame *meta_view)
{
  g_return_if_fail (!g_list_find (window->meta_views, meta_view));
  g_return_if_fail (NAUTILUS_IS_META_VIEW_FRAME (meta_view));

  nautilus_index_panel_add_meta_view (window->index_panel, meta_view);
  window->meta_views = g_list_prepend (window->meta_views, meta_view);
}

void
nautilus_window_remove_meta_view_real (NautilusWindow *window, NautilusViewFrame *meta_view)
{
  nautilus_index_panel_remove_meta_view(window->index_panel, meta_view);
}

void
nautilus_window_remove_meta_view (NautilusWindow *window, NautilusViewFrame *meta_view)
{
  if (!g_list_find(window->meta_views, meta_view))
    return;

  window->meta_views = g_list_remove(window->meta_views, meta_view);
  nautilus_window_remove_meta_view_real(window, meta_view);
}

void
nautilus_window_back_or_forward (NautilusWindow *window, gboolean back, guint distance)
{
  Nautilus_NavigationRequestInfo nri;
  GSList *list;

  list = back ? window->back_list : window->forward_list;
  g_assert (g_slist_length (list) > distance);

  memset(&nri, 0, sizeof(nri));
  /* FIXME bugzilla.eazel.com 608: 
   * Have to cast away the const for nri.requested_uri. This field should be
   * declared const. 
   */
  nri.requested_uri = (char *)nautilus_bookmark_get_uri (g_slist_nth_data (list, distance));
  nri.new_window_requested = FALSE;

  nautilus_window_begin_location_change (window, &nri, NULL, back ? NAUTILUS_LOCATION_CHANGE_BACK : NAUTILUS_LOCATION_CHANGE_FORWARD, distance);
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

const char *
nautilus_window_get_requested_uri (NautilusWindow *window)
{
  return window->ni == NULL ? NULL : window->ni->requested_uri;
}

BonoboUIHandler *
nautilus_window_get_uih(NautilusWindow *window)
{
  return window->uih;
}

void
nautilus_window_go_up (NautilusWindow *window)
{
  const char *requested_uri;
  GnomeVFSURI *current_uri;
  GnomeVFSURI *parent_uri;
  char *parent_uri_string;

  requested_uri = nautilus_window_get_requested_uri(window);
  if (requested_uri == NULL)
    return;

  current_uri = gnome_vfs_uri_new (requested_uri);
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
  nautilus_window_set_initial_state(window, NULL);
}


void
nautilus_window_allow_back (NautilusWindow *window, gboolean allow)
{
   gtk_widget_set_sensitive(window->back_button, allow); 
   bonobo_ui_handler_menu_set_sensitivity(window->uih, NAUTILUS_MENU_PATH_BACK_ITEM, allow);
}

void
nautilus_window_allow_forward (NautilusWindow *window, gboolean allow)
{
   gtk_widget_set_sensitive(window->forward_button, allow); 
   bonobo_ui_handler_menu_set_sensitivity(window->uih, NAUTILUS_MENU_PATH_FORWARD_ITEM, allow);
}

void
nautilus_window_allow_up (NautilusWindow *window, gboolean allow)
{
   gtk_widget_set_sensitive(window->up_button, allow); 
   bonobo_ui_handler_menu_set_sensitivity(window->uih, NAUTILUS_MENU_PATH_UP_ITEM, allow);
}

void
nautilus_window_allow_reload (NautilusWindow *window, gboolean allow)
{
   gtk_widget_set_sensitive(window->reload_button, allow); 
}

void
nautilus_window_allow_stop (NautilusWindow *window, gboolean allow)
{
  gtk_widget_set_sensitive(window->stop_button, allow); 
}

void
nautilus_send_history_list_changed ()
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
nautilus_get_history_list ()
{
  return history_list;
}


static void
nautilus_window_request_location_change_cb (NautilusViewFrame *view, 
                                            Nautilus_NavigationRequestInfo *info, 
                                            NautilusWindow *window)
{
  nautilus_window_request_location_change(window, info, view);
}


static void
nautilus_window_request_selection_change_cb (NautilusViewFrame *view, 
                                             Nautilus_SelectionRequestInfo *info, 
                                             NautilusWindow *window)
{
  nautilus_window_request_selection_change(window, info, view);  
}

static void
nautilus_window_request_status_change_cb (NautilusViewFrame *view,
                                          Nautilus_StatusRequestInfo *info,
                                          NautilusWindow *window)
{
  nautilus_window_request_status_change(window, info, view);  
}

static void
nautilus_window_request_progress_change_cb (NautilusViewFrame *view,
                                            Nautilus_ProgressRequestInfo *info,
                                            NautilusWindow *window)
{
  nautilus_window_request_progress_change(window, info, view);  
}

static void
nautilus_window_request_title_change_callback (NautilusContentViewFrame *view,
                                               const char *new_title,
                                               NautilusWindow *window)
{
  nautilus_window_request_title_change(window, new_title, view);  
}

void
nautilus_window_connect_view(NautilusWindow *window, NautilusViewFrame *view)
{
  GtkObject *view_object;

  view_object = GTK_OBJECT(view);
  gtk_signal_connect(view_object,
                     "request_location_change", 
                     nautilus_window_request_location_change_cb, 
                     window);
  gtk_signal_connect(view_object, 
                     "request_selection_change", 
                     nautilus_window_request_selection_change_cb, 
                     window);
  gtk_signal_connect(view_object, 
                     "request_status_change", 
                     nautilus_window_request_status_change_cb, 
                     window);
  gtk_signal_connect(view_object, 
                     "request_progress_change", 
                     nautilus_window_request_progress_change_cb, 
                     window);
  gtk_signal_connect(view_object,
                     "destroy",
                     nautilus_window_view_destroyed,
                     window);
}

void
nautilus_window_connect_content_view(NautilusWindow *window, NautilusContentViewFrame *view)
{
  GtkObject *view_object;

  /* First connect with NautilusViewFrame signals. */
  nautilus_window_connect_view (window, NAUTILUS_VIEW_FRAME (view));

  /* Now connect with NautilusContentViewFrame signals. */
  view_object = GTK_OBJECT(view);
  gtk_signal_connect(view_object,
                     "request_title_change", 
                     nautilus_window_request_title_change_callback, 
                     window);
}

void
nautilus_window_display_error(NautilusWindow *window, const char *error_msg)
{
  GtkWidget *dialog;

  dialog = gnome_message_box_new(error_msg, GNOME_MESSAGE_BOX_ERROR, _("Close"), NULL);
  gnome_dialog_set_close(GNOME_DIALOG(dialog), TRUE);

  gnome_dialog_set_default(GNOME_DIALOG(dialog), 0);

  gtk_widget_show(dialog);
}

static void
nautilus_window_real_set_content_view (NautilusWindow *window, NautilusViewFrame *new_view)
{
  g_return_if_fail (NAUTILUS_IS_WINDOW (window));
  g_return_if_fail (new_view == NULL || NAUTILUS_IS_VIEW_FRAME (new_view));

  if (new_view == window->content_view)
    {
      return;
    }

  if (window->content_view != NULL)
    {
      gtk_container_remove (GTK_CONTAINER (window->content_hbox), GTK_WIDGET (window->content_view));      
    }

  if (new_view != NULL)
    {
      nautilus_zoom_control_reset_zoom_level (NAUTILUS_ZOOM_CONTROL (window->zoom_control));

      gtk_widget_show (GTK_WIDGET (new_view));

      nautilus_content_view_frame_set_active (NAUTILUS_CONTENT_VIEW_FRAME (new_view)); 

      gtk_paned_pack2(GTK_PANED(window->content_hbox), GTK_WIDGET (new_view), TRUE, FALSE);
    }
      
  gtk_widget_queue_resize(window->content_hbox);
  window->content_view = new_view;
}
