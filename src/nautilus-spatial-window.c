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

#include "config.h"
#include <gnome.h>
#include "nautilus.h"
#include "nautilus-bookmarks-menu.h"
#include "explorer-location-bar.h"
#include "ntl-index-panel.h"
#include "ntl-window-private.h"
#include "ntl-miniicon.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libnautilus/nautilus-gtk-extensions.h>

static void nautilus_window_realize (GtkWidget *widget);
static void nautilus_window_real_set_content_view (NautilusWindow *window, NautilusView *new_view);

static GnomeAppClass *parent_class = NULL;

/* Stuff for handling the CORBA interface */
typedef struct {
  POA_Nautilus_ViewWindow servant;
  gpointer gnome_object;

  NautilusWindow *window;
} impl_POA_Nautilus_ViewWindow;

static POA_Nautilus_ViewWindow__epv impl_Nautilus_ViewWindow_epv =
{
  NULL
};

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

static POA_Nautilus_ViewWindow__vepv impl_Nautilus_ViewWindow_vepv =
{
   &base_epv,
   NULL,
   &impl_Nautilus_ViewWindow_epv
};


static void
impl_Nautilus_ViewWindow__destroy(GnomeObject *obj, impl_POA_Nautilus_ViewWindow *servant)
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

static GnomeObject *
impl_Nautilus_ViewWindow__create(NautilusWindow *window)
{
   GnomeObject *retval;
   impl_POA_Nautilus_ViewWindow *newservant;
   CORBA_Environment ev;

   CORBA_exception_init(&ev);

   newservant = g_new0(impl_POA_Nautilus_ViewWindow, 1);
   newservant->servant.vepv = &impl_Nautilus_ViewWindow_vepv;
   newservant->window = window;
   POA_Nautilus_ViewWindow__init((PortableServer_Servant) newservant, &ev);

   retval = gnome_object_new_from_servant(newservant);

   gtk_signal_connect(GTK_OBJECT(retval), "destroy", GTK_SIGNAL_FUNC(impl_Nautilus_ViewWindow__destroy), newservant);
   CORBA_exception_free(&ev);

   return retval;
}

static void nautilus_window_class_init (NautilusWindowClass *klass);
static void nautilus_window_init (NautilusWindow *window);
static void nautilus_window_destroy (NautilusWindow *window);
static void nautilus_window_back (GtkWidget *btn, NautilusWindow *window);
static void nautilus_window_fwd (GtkWidget *btn, NautilusWindow *window);
static void nautilus_window_up (GtkWidget *btn, NautilusWindow *window);
static void nautilus_window_reload (GtkWidget *btn, NautilusWindow *window);
static void nautilus_window_home (GtkWidget *btn, NautilusWindow *window);
static void nautilus_window_color_confirm (GtkWidget *widget);
static void nautilus_window_show_color_picker (GtkWidget *btn, NautilusWindow *window);
static void nautilus_window_stop (GtkWidget *btn, NautilusWindow *window);
static void nautilus_window_set_arg (GtkObject      *object,
                                     GtkArg         *arg,
                                     guint	      arg_id);
static void nautilus_window_get_arg (GtkObject      *object,
                                     GtkArg         *arg,
                                     guint	      arg_id);
static void nautilus_window_goto_uri_cb (GtkWidget *widget,
                                         const char *uri,
                                         GtkWidget *window);
static void nautilus_window_about_cb (GtkWidget *widget,
                                      NautilusWindow *window);

#define CONTENTS_AS_HBOX 1

/* milliseconds */
#define STATUSBAR_CLEAR_TIMEOUT 5000

/* menu definitions */

static void
file_menu_close_cb (GtkWidget *widget,
	       gpointer data)
{
	GtkWidget *window;

	window = GTK_WIDGET (data);
	gtk_widget_destroy(window);
}

static void
file_menu_new_window_cb (GtkWidget *widget,
                         gpointer data)
{
  NautilusWindow *current_mainwin;
  NautilusWindow *new_mainwin;

  g_return_if_fail(NAUTILUS_IS_WINDOW(data));
  
  current_mainwin = NAUTILUS_WINDOW(data);

  new_mainwin = nautilus_app_create_window();

  nautilus_window_goto_uri(new_mainwin, 
                           nautilus_window_get_requested_uri(current_mainwin));

  gtk_widget_show(GTK_WIDGET(new_mainwin));
}

static void
file_menu_exit_cb (GtkWidget *widget,
                   gpointer data)
{
  gtk_main_quit();
}

static void
edit_menu_prefs_cb(GtkWidget *widget,
                   GtkWindow *mainwin)
{
  nautilus_prefs_ui_show(mainwin);
}

static GnomeUIInfo file_menu_info[] = {
  {
    GNOME_APP_UI_ITEM,
    N_("New Window"), N_("Create a new window"),
    file_menu_new_window_cb, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    'N', GDK_CONTROL_MASK, NULL
  },
  GNOMEUIINFO_MENU_CLOSE_ITEM(file_menu_close_cb, NULL),
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_EXIT_ITEM(file_menu_exit_cb, NULL),
  GNOMEUIINFO_END
};

/* FIXME: These all need implementation, though we might end up doing that
 * separately for each content view (and merging with the insensitive items here)
 */
static GnomeUIInfo edit_menu_info[] = {
  GNOMEUIINFO_MENU_UNDO_ITEM(NULL, NULL),
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_CUT_ITEM(NULL, NULL),
  GNOMEUIINFO_MENU_COPY_ITEM(NULL, NULL),
  GNOMEUIINFO_MENU_PASTE_ITEM(NULL, NULL),
  GNOMEUIINFO_MENU_CLEAR_ITEM(NULL, NULL),
  GNOMEUIINFO_SEPARATOR,
  /* Didn't use standard SELECT_ALL_ITEM 'cuz it didn't have accelerator */
  {
    GNOME_APP_UI_ITEM,
    N_("Select All"), NULL,
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    'A', GDK_CONTROL_MASK, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_PREFERENCES_ITEM(edit_menu_prefs_cb, NULL),
  GNOMEUIINFO_END
};

static GnomeUIInfo bookmarks_menu_info[] = {
  GNOMEUIINFO_END
};

static GnomeUIInfo help_menu_info[] = {
  {
    GNOME_APP_UI_ITEM,
    N_("About Nautilus..."), N_("Info about the Nautilus program"),
    nautilus_window_about_cb, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_ABOUT,
    0, 0, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo debug_menu_info [] = {
	GNOMEUIINFO_ITEM_NONE (N_("Show Color selector..."), N_("Show the color picker window"), nautilus_window_show_color_picker),
	GNOMEUIINFO_END
};


#define BOOKMARKS_MENU_INDEX	2
static GnomeUIInfo main_menu[] = {
  GNOMEUIINFO_MENU_FILE_TREE (file_menu_info),
  GNOMEUIINFO_MENU_EDIT_TREE (edit_menu_info),
  GNOMEUIINFO_SUBTREE(N_("_Bookmarks"), bookmarks_menu_info),
  GNOMEUIINFO_MENU_HELP_TREE (help_menu_info),
  GNOMEUIINFO_SUBTREE(N_("_Debug"), debug_menu_info),
  GNOMEUIINFO_END
};

/* toolbar definitions */

static GnomeUIInfo toolbar_info[] = {
  GNOMEUIINFO_ITEM_STOCK
  (N_("Back"), N_("Go to the previously visited directory"),
   nautilus_window_back, GNOME_STOCK_PIXMAP_BACK),
  GNOMEUIINFO_ITEM_STOCK
  (N_("Forward"), N_("Go to the next directory"),
   nautilus_window_fwd, GNOME_STOCK_PIXMAP_FORWARD),
  GNOMEUIINFO_ITEM_STOCK
  (N_("Up"), N_("Go up a level in the directory heirarchy"),
   nautilus_window_up, GNOME_STOCK_PIXMAP_UP),
  GNOMEUIINFO_ITEM_STOCK
  (N_("Reload"), N_("Reload this view"),
   nautilus_window_reload, GNOME_STOCK_PIXMAP_REFRESH),
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_ITEM_STOCK
  (N_("Home"), N_("Go to your home directory"),
   nautilus_window_home, GNOME_STOCK_PIXMAP_HOME),
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_ITEM_STOCK
  (N_("Stop"), N_("Interrupt loading"),
   nautilus_window_stop, GNOME_STOCK_PIXMAP_STOP),
  GNOMEUIINFO_END
};

	
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
  ARG_CONTENT_VIEW
};

static void
nautilus_window_class_init (NautilusWindowClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  int i;

  parent_class = gtk_type_class(gnome_app_get_type());
  
  object_class = (GtkObjectClass*) klass;
  object_class->destroy = (void (*)(GtkObject *))nautilus_window_destroy;
  object_class->get_arg = nautilus_window_get_arg;
  object_class->set_arg = nautilus_window_set_arg;

  widget_class = (GtkWidgetClass*) klass;
  klass->parent_class = gtk_type_class (gtk_type_parent (object_class->type));

  i = 0;
  gtk_object_class_add_signals (object_class, klass->window_signals, i);

  gtk_object_add_arg_type ("NautilusWindow::app_id",
			   GTK_TYPE_STRING,
			   GTK_ARG_READWRITE|GTK_ARG_CONSTRUCT,
			   ARG_APP_ID);
  gtk_object_add_arg_type ("NautilusWindow::content_view",
			   GTK_TYPE_OBJECT,
			   GTK_ARG_READWRITE,
			   ARG_CONTENT_VIEW);
  impl_Nautilus_ViewWindow_vepv.GNOME_Unknown_epv = gnome_object_get_epv();

  widget_class->realize = nautilus_window_realize;
}

static void
nautilus_window_init (NautilusWindow *window)
{
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
  if(txt)
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
  navinfo.new_window_default = navinfo.new_window_suggested = Nautilus_V_FALSE;
  navinfo.new_window_enforced = Nautilus_V_UNKNOWN;

  nautilus_window_request_location_change (window, &navinfo, NULL);
  nautilus_index_panel_set_uri (NAUTILUS_INDEX_PANEL (window->index_panel), uri); 
}

static void
nautilus_window_goto_uri_cb (GtkWidget *widget,
                             const char *uri,
                             GtkWidget *window)
{
  nautilus_window_goto_uri(NAUTILUS_WINDOW(window), uri);
}

static void
nautilus_window_constructed(NautilusWindow *window)
{
  GnomeApp *app;
  GtkWidget *location_bar_box, *statusbar;
  GtkWidget *temp_frame;
  
  app = GNOME_APP(window);

  /* set up toolbar */
  gnome_app_create_toolbar_with_data(app, toolbar_info, window);
  window->btn_back = toolbar_info[0].widget;
  window->btn_fwd = toolbar_info[1].widget;
  nautilus_window_allow_stop(window, FALSE);
  gtk_widget_set_sensitive(window->btn_back, FALSE);
  gtk_widget_set_sensitive(window->btn_fwd, FALSE);

  /* set up location bar */

  location_bar_box = gtk_hbox_new(FALSE, GNOME_PAD);
  gtk_container_set_border_width(GTK_CONTAINER(location_bar_box), GNOME_PAD_SMALL);

  window->ent_uri = explorer_location_bar_new();
  gtk_signal_connect(GTK_OBJECT(window->ent_uri), "location_changed",
                     nautilus_window_goto_uri_cb, window);
  gtk_box_pack_start(GTK_BOX(location_bar_box), window->ent_uri, TRUE, TRUE, GNOME_PAD);
  gnome_app_add_docked(app, location_bar_box, "uri-entry",
  					   GNOME_DOCK_ITEM_BEH_EXCLUSIVE|GNOME_DOCK_ITEM_BEH_NEVER_VERTICAL,
                       GNOME_DOCK_TOP, 2, 0, 0);

  /* Option menu for content view types; it's empty here, filled in when a uri is set. */
  window->option_cvtype = gtk_option_menu_new();
  gtk_box_pack_end(GTK_BOX(location_bar_box), window->option_cvtype, FALSE, FALSE, GNOME_PAD_BIG);
  gtk_widget_show(window->option_cvtype);

  gtk_widget_show_all(location_bar_box);

  /* set up status bar */

  gnome_app_set_statusbar(app, (statusbar = gtk_statusbar_new()));
  /* insert a little padding so text isn't jammed against frame */
  gtk_misc_set_padding(GTK_MISC ((GTK_STATUSBAR (statusbar))->label), GNOME_PAD, 0);
  window->statusbar_ctx = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar), "IhateGtkStatusbar");

  /* set up window contents and policy */

  gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);
  gtk_window_set_default_size(GTK_WINDOW(window), 650, 400);

#ifdef CONTENTS_AS_HBOX
  window->content_hbox = gtk_hbox_new(FALSE, 0);
#else
  window->content_hbox = gtk_hpaned_new();
#endif
  gnome_app_set_contents(app, window->content_hbox);

  /* set up the index panel in a frame */

  temp_frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(temp_frame), GTK_SHADOW_OUT);
  gtk_widget_show(temp_frame);
  
  window->index_panel = nautilus_index_panel_new();
  gtk_widget_show (GTK_WIDGET (window->index_panel));
  gtk_container_add(GTK_CONTAINER(temp_frame), GTK_WIDGET (window->index_panel));

#ifdef CONTENTS_AS_HBOX
  gtk_box_pack_start(GTK_BOX(window->content_hbox), temp_frame, FALSE, FALSE, 0);
#else
  gtk_paned_pack1(GTK_PANED(window->content_hbox), window->index_panel, TRUE, TRUE);
#endif

  gtk_widget_show_all(window->content_hbox);

  /* CORBA stuff */
  window->ntl_viewwindow = impl_Nautilus_ViewWindow__create(window);
  window->uih = gnome_ui_handler_new();
  gnome_ui_handler_set_app(window->uih, app);
  gnome_ui_handler_set_statusbar(window->uih, statusbar);

  /* set up menu bar */
  gnome_app_create_menus_with_data(app, main_menu, window);

  /* desensitize the items that haven't yet been implemented
   * FIXME: these all need to be implemented. I'm using hardwired numbers
   * rather than enum symbols here just 'cuz the enums aren't needed (I think)
   * once we've implemented things. If it turns out they are, we'll define 'em.
   */

  gtk_widget_set_sensitive(edit_menu_info[0].widget, FALSE); /* Undo */
  gtk_widget_set_sensitive(edit_menu_info[2].widget, FALSE); /* Cut */
  gtk_widget_set_sensitive(edit_menu_info[3].widget, FALSE); /* Copy */
  gtk_widget_set_sensitive(edit_menu_info[4].widget, FALSE); /* Paste */
  gtk_widget_set_sensitive(edit_menu_info[5].widget, FALSE); /* Clear */
  gtk_widget_set_sensitive(edit_menu_info[7].widget, FALSE); /* Select All */

  /* insert bookmarks menu */
  gtk_menu_item_set_submenu(GTK_MENU_ITEM (main_menu[BOOKMARKS_MENU_INDEX].widget), 
  			    nautilus_bookmarks_menu_new(window));
  gnome_app_install_menu_hints(app, main_menu); /* This has to go here
                                                   after the statusbar
                                                   creation */
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
    if(!old_app_name)
      nautilus_window_constructed(NAUTILUS_WINDOW(object));
    break;
  case ARG_CONTENT_VIEW:
    nautilus_window_real_set_content_view (window, (NautilusView *)GTK_VALUE_OBJECT(*arg));
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
  case ARG_CONTENT_VIEW:
    GTK_VALUE_OBJECT(*arg) = GTK_OBJECT(((NautilusWindow *)object)->content_view);
    break;
  }
}

static void nautilus_window_destroy (NautilusWindow *window)
{
  NautilusWindowClass *klass = NAUTILUS_WINDOW_CLASS(GTK_OBJECT(window)->klass);

  g_slist_free(window->meta_views);
  CORBA_free(window->ni);
  CORBA_free(window->si);
  g_slist_foreach(window->uris_prev, (GFunc)g_free, NULL);
  g_slist_foreach(window->uris_next, (GFunc)g_free, NULL);
  g_slist_free(window->uris_prev);
  g_slist_free(window->uris_next);

  if(window->statusbar_clear_id)
    g_source_remove(window->statusbar_clear_id);

  if(GTK_OBJECT_CLASS(klass->parent_class)->destroy)
    GTK_OBJECT_CLASS(klass->parent_class)->destroy(GTK_OBJECT(window));
}

GtkWidget *
nautilus_window_new(const char *app_id)
{
  return GTK_WIDGET (gtk_object_new (nautilus_window_get_type(), "app_id", app_id, NULL));
}

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
        /* FIXME draw a real icon */
        /* FIXME The icon should be 16x16, we get garbage on the edges
           since it's 12x12 */
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

        /* FIXME I think we are leaking the pixmap/mask here */
}

/*
 * Main API
 */

#if 0
static gboolean
nautilus_window_send_show_properties(GtkWidget *dockitem, GdkEventButton *event, NautilusView *meta_view)
{
  if(event->button != 3)
    return FALSE;

  gtk_signal_emit_stop_by_name(GTK_OBJECT(dockitem), "button_press_event");

  nautilus_view_show_properties(meta_view);

  return TRUE;
}
#endif

void
nautilus_window_set_content_view(NautilusWindow *window, NautilusView *content_view)
{
  nautilus_window_real_set_content_view (window, content_view);
}

void
nautilus_window_add_meta_view(NautilusWindow *window, NautilusView *meta_view)
{
  g_return_if_fail (!g_slist_find (window->meta_views, meta_view));
  g_return_if_fail (NAUTILUS_IS_META_VIEW (meta_view));

  nautilus_index_panel_add_meta_view (window->index_panel, meta_view);
  window->meta_views = g_slist_prepend (window->meta_views, meta_view);
}

void
nautilus_window_remove_meta_view_real(NautilusWindow *window, NautilusView *meta_view)
{
  nautilus_index_panel_remove_meta_view(window->index_panel, meta_view);
}

void
nautilus_window_remove_meta_view(NautilusWindow *window, NautilusView *meta_view)
{
  if(!g_slist_find(window->meta_views, meta_view))
    return;

  window->meta_views = g_slist_remove(window->meta_views, meta_view);
  nautilus_window_remove_meta_view_real(window, meta_view);
}

/* FIXME: Factor toolbar stuff out into ntl-window-toolbar.c */

static void
nautilus_window_back (GtkWidget *btn, NautilusWindow *window)
{
  Nautilus_NavigationRequestInfo nri;

  g_assert(window->uris_prev);

  memset(&nri, 0, sizeof(nri));
  nri.requested_uri = window->uris_prev->data;
  nri.new_window_default = nri.new_window_suggested = nri.new_window_enforced = Nautilus_V_FALSE;

  nautilus_window_change_location(window, &nri, NULL, TRUE, FALSE);
}

static void
nautilus_window_fwd (GtkWidget *btn, NautilusWindow *window)
{
  Nautilus_NavigationRequestInfo nri;

  g_assert(window->uris_next);

  memset(&nri, 0, sizeof(nri));
  nri.requested_uri = window->uris_next->data;
  nri.new_window_default = nri.new_window_suggested = nri.new_window_enforced = Nautilus_V_FALSE;
  nautilus_window_change_location(window, &nri, NULL, FALSE, FALSE);
}

const char *
nautilus_window_get_requested_uri (NautilusWindow *window)
{
  return window->ni == NULL ? NULL : window->ni->requested_uri;
}

GnomeUIHandler *
nautilus_window_get_uih(NautilusWindow *window)
{
  return window->uih;
}

static void
nautilus_window_up (GtkWidget *btn, NautilusWindow *window)
{
  GnomeVFSURI *current_uri;
  GnomeVFSURI *parent_uri;
  char *parent_uri_string;

  current_uri = gnome_vfs_uri_new (nautilus_window_get_requested_uri(window));
  parent_uri = gnome_vfs_uri_get_parent (current_uri);
  gnome_vfs_uri_unref (current_uri);
  parent_uri_string = gnome_vfs_uri_to_string (parent_uri, GNOME_VFS_URI_HIDE_NONE);
  gnome_vfs_uri_unref (parent_uri);  
  
  nautilus_window_goto_uri (window, parent_uri_string);
  
  g_free (parent_uri_string);
}

static void
nautilus_window_reload (GtkWidget *btn, NautilusWindow *window)
{
  Nautilus_NavigationRequestInfo nri;

  memset(&nri, 0, sizeof(nri));
  nri.requested_uri = (char *)nautilus_window_get_requested_uri(window);
  nri.new_window_default = nri.new_window_suggested = nri.new_window_enforced = Nautilus_V_FALSE;
  nautilus_window_change_location(window, &nri, NULL, FALSE, TRUE);
}

static void
nautilus_window_home (GtkWidget *btn, NautilusWindow *window)
{
  nautilus_window_set_initial_state(window, NULL);
}

/* handle the OK button being pushed on the color selector */
/* for now, just vanquish it, since it's only for testing */
static void
nautilus_window_color_confirm (GtkWidget *widget)
{
	gtk_widget_destroy (gtk_widget_get_toplevel (widget));
}

static void nautilus_window_show_color_picker (GtkWidget *btn, NautilusWindow *window)
{
	GtkWidget *c;

	c = gtk_color_selection_dialog_new (_("Color selector"));
	gtk_signal_connect (GTK_OBJECT (GTK_COLOR_SELECTION_DIALOG (c)->ok_button),
			    "clicked", GTK_SIGNAL_FUNC (nautilus_window_color_confirm), c);
	gtk_widget_hide (GTK_COLOR_SELECTION_DIALOG (c)->cancel_button);
	gtk_widget_hide (GTK_COLOR_SELECTION_DIALOG (c)->help_button);
	gtk_widget_show (c);
}


static void
nautilus_window_stop (GtkWidget *btn, NautilusWindow *window)
{
  nautilus_window_set_state_info(window, RESET_TO_IDLE, 0);
}


/**
 * nautilus_window_about_cb:
 * 
 * Display about box, creating it first if necessary. Callback used when 
 * user selects "About Nautilus".
 * @widget: ignored
 * @window: ignored
 **/
static void
nautilus_window_about_cb (GtkWidget *widget,
                          NautilusWindow *window)
{
  static GtkWidget *aboot = NULL;

  if (aboot == NULL)
  {
    const char *authors[] = {
      "Darin Adler",
      "Andy Hertzfeld",
      "Elliot Lee",
      "Ettore Perazzoli",
      "Maciej Stachowiak",
      "John Sullivan",
      NULL
    };

    aboot = gnome_about_new(_("Nautilus"),
                            VERSION,
                            "Copyright (C) 1999, 2000",
                            authors,
                            _("The Cool Shell Program"),
                            "nautilus/nautilus3.jpg");

    gnome_dialog_close_hides (GNOME_DIALOG (aboot), TRUE);
  }

  nautilus_gtk_window_present (GTK_WINDOW (aboot));
}

void
nautilus_window_allow_back (NautilusWindow *window, gboolean allow)
{
   gtk_widget_set_sensitive(toolbar_info[0].widget, allow); 
}

void
nautilus_window_allow_forward (NautilusWindow *window, gboolean allow)
{
   gtk_widget_set_sensitive(toolbar_info[1].widget, allow); 
}

void
nautilus_window_allow_up (NautilusWindow *window, gboolean allow)
{
   gtk_widget_set_sensitive(toolbar_info[2].widget, allow); 
}

void
nautilus_window_allow_reload (NautilusWindow *window, gboolean allow)
{
   gtk_widget_set_sensitive(toolbar_info[5].widget, allow); 
}

void
nautilus_window_allow_stop (NautilusWindow *window, gboolean allow)
{
   gtk_widget_set_sensitive(toolbar_info[7].widget, allow); 
}


static void
nautilus_window_request_location_change_cb (NautilusView *view, 
                                            Nautilus_NavigationRequestInfo *info, 
                                            NautilusWindow *window)
{
  nautilus_window_request_location_change(window, info, view);
}


static void
nautilus_window_request_selection_change_cb (NautilusView *view, 
                                             Nautilus_SelectionRequestInfo *info, 
                                             NautilusWindow *window)
{
  nautilus_window_request_selection_change(window, info, view);  
}

static void
nautilus_window_request_status_change_cb (NautilusView *view,
                                          Nautilus_StatusRequestInfo *info,
                                          NautilusWindow *window)
{
  nautilus_window_request_status_change(window, info, view);  
}

static void
nautilus_window_request_progress_change_cb (NautilusView *view,
                                            Nautilus_ProgressRequestInfo *info,
                                            NautilusWindow *window)
{
  nautilus_window_request_progress_change(window, info, view);  
}

void
nautilus_window_connect_view(NautilusWindow *window, NautilusView *view)
{
  GtkObject *viewo;

  viewo = GTK_OBJECT(view);
  gtk_signal_connect(viewo,
                     "request_location_change", 
                     nautilus_window_request_location_change_cb, 
                     window);
  gtk_signal_connect(viewo, 
                     "request_selection_change", 
                     nautilus_window_request_selection_change_cb, 
                     window);
  gtk_signal_connect(viewo, 
                     "request_status_change", 
                     nautilus_window_request_status_change_cb, 
                     window);
  gtk_signal_connect(viewo, 
                     "request_progress_change", 
                     nautilus_window_request_progress_change_cb, 
                     window);
  gtk_signal_connect(viewo,
                     "destroy",
                     nautilus_window_view_destroyed,
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
nautilus_window_real_set_content_view (NautilusWindow *window, NautilusView *new_view)
{
  g_return_if_fail (NAUTILUS_IS_WINDOW (window));
  g_return_if_fail (new_view == NULL || NAUTILUS_IS_VIEW (new_view));

  if (new_view == window->content_view)
    {
      return;
    }

  if (window->content_view != NULL)
    {
      gtk_container_remove(GTK_CONTAINER(window->content_hbox), GTK_WIDGET(window->content_view));      
    }

  if (new_view != NULL)
    {
      gtk_widget_show (GTK_WIDGET (new_view));
#ifdef CONTENTS_AS_HBOX
      gtk_box_pack_end(GTK_BOX(window->content_hbox), GTK_WIDGET (new_view), TRUE, TRUE, 0);
#else
      gtk_paned_pack2(GTK_PANED(window->content_hbox), GTK_WIDGET (new_view), TRUE, FALSE);
#endif      
    }
      
  gtk_widget_queue_resize(window->content_hbox);
  window->content_view = new_view;
}

