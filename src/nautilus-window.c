/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999 Red Hat, Inc.
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
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */
/* ntl-window.c: Implementation of the main window object */

#include "config.h"
#include <gnome.h>
#include "nautilus.h"
#include "explorer-location-bar.h"
#include "ntl-window-private.h"

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
static void nautilus_window_set_arg (GtkObject      *object,
                                     GtkArg         *arg,
                                     guint	      arg_id);
static void nautilus_window_get_arg (GtkObject      *object,
                                     GtkArg         *arg,
                                     guint	      arg_id);
static void nautilus_window_goto_url_cb (GtkWidget *widget,
                                         const char *url,
                                         GtkWidget *window);

#define CONTENTS_AS_HBOX
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

  object_class = (GtkObjectClass*) klass;
  object_class->destroy = (void (*)(GtkObject *))nautilus_window_destroy;
  object_class->get_arg = nautilus_window_get_arg;
  object_class->set_arg = nautilus_window_set_arg;

  widget_class = (GtkWidgetClass*) klass;
  klass->parent_class = gtk_type_class (gtk_type_parent (object_class->type));

  klass->request_location_change = nautilus_window_real_request_location_change;
  klass->request_selection_change = nautilus_window_real_request_selection_change;
  klass->request_status_change = nautilus_window_real_request_status_change;

  i = 0;
  klass->window_signals[i++] = gtk_signal_new("request_location_change",
					      GTK_RUN_LAST,
					      object_class->type,
					      GTK_SIGNAL_OFFSET (NautilusWindowClass, request_location_change),
					      gtk_marshal_NONE__BOXED_OBJECT,
					      GTK_TYPE_NONE, 2, GTK_TYPE_BOXED, GTK_TYPE_OBJECT);
  klass->window_signals[i++] = gtk_signal_new("request_selection_change",
					      GTK_RUN_LAST,
					      object_class->type,
					      GTK_SIGNAL_OFFSET (NautilusWindowClass, request_selection_change),
					      gtk_marshal_NONE__BOXED_OBJECT,
					      GTK_TYPE_NONE, 2, GTK_TYPE_BOXED, GTK_TYPE_OBJECT);
  klass->window_signals[i++] = gtk_signal_new("request_status_change",
					      GTK_RUN_LAST,
					      object_class->type,
					      GTK_SIGNAL_OFFSET (NautilusWindowClass, request_status_change),
					      gtk_marshal_NONE__BOXED_OBJECT,
					      GTK_TYPE_NONE, 2, GTK_TYPE_BOXED, GTK_TYPE_OBJECT);
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
nautilus_window_goto_url(NautilusWindow *window, const char *url)
{
  Nautilus_NavigationRequestInfo navinfo;

  memset(&navinfo, 0, sizeof(navinfo));
  navinfo.requested_uri = (char *)url;
  navinfo.new_window_default = navinfo.new_window_suggested = Nautilus_V_FALSE;
  navinfo.new_window_enforced = Nautilus_V_UNKNOWN;

  nautilus_window_request_location_change(window, &navinfo, NULL);
}

static void
nautilus_window_goto_url_cb (GtkWidget *widget,
                          const char *url,
                          GtkWidget *window)
{
  nautilus_window_goto_url(NAUTILUS_WINDOW(window), url);
}

static void
gtk_option_menu_do_resize(GtkWidget *widget, GtkWidget *child, GtkWidget *optmenu)
{
  GtkRequisition req;
  gtk_widget_size_request(widget, &req);

  gtk_widget_set_usize(optmenu, req.width, -1);
}

static void
nautilus_window_constructed(NautilusWindow *window)
{
  GnomeApp *app;
  GnomeUIInfo main_menu[] = {
    GNOMEUIINFO_END
  };
  GtkWidget *menu_hbox, *menubar, *wtmp, *statusbar;
  GtkAccelGroup *ag;

  app = GNOME_APP(window);

  ag = gtk_accel_group_new();
  gtk_object_set_data(GTK_OBJECT(app), "GtkAccelGroup", ag);
  gtk_window_add_accel_group(GTK_WINDOW(app), ag);

  menu_hbox = gtk_hbox_new(FALSE, GNOME_PAD);

  menubar = gtk_menu_bar_new();
  gtk_box_pack_end(GTK_BOX(menu_hbox), menubar, FALSE, TRUE, GNOME_PAD_BIG);
  gnome_app_fill_menu_with_data(GTK_MENU_SHELL(menubar), main_menu, ag, TRUE, 0, window);

  window->option_cvtype = gtk_option_menu_new();
  window->menu_cvtype = gtk_menu_new();
  // FIXME: Add in placeholder item for now; rework this when we get the right views showing up.
  // Since menu doesn't yet work, make it insensitive.
  gtk_container_add(GTK_CONTAINER(window->menu_cvtype), 
  	gtk_menu_item_new_with_label(_("View as (placeholder)")));
  gtk_widget_set_sensitive(window->option_cvtype, FALSE);
  
  gtk_widget_show_all(window->menu_cvtype);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(window->option_cvtype), window->menu_cvtype);
  gtk_option_menu_set_history(GTK_OPTION_MENU(window->option_cvtype), 0);
  gtk_box_pack_end(GTK_BOX(menu_hbox), window->option_cvtype, FALSE, FALSE, GNOME_PAD_BIG);
  gtk_widget_show(window->option_cvtype);

  // For mysterious reasons, connecting these signals before laying out the menu
  // and option menu ends up making the option menu not know how to size itself (i.e, 
  // it is zero-width unless you turn on expand and fill). So we do it here afterwards.
  gtk_signal_connect_while_alive(GTK_OBJECT(window->menu_cvtype), "add",
                                 GTK_SIGNAL_FUNC(gtk_option_menu_do_resize), window->option_cvtype,
                                 GTK_OBJECT(window->option_cvtype));
  gtk_signal_connect_while_alive(GTK_OBJECT(window->menu_cvtype), "remove",
                                 GTK_SIGNAL_FUNC(gtk_option_menu_do_resize), window->option_cvtype,
                                 GTK_OBJECT(window->option_cvtype));



  /* A hacked-up version of gnome_app_set_menu(). We need this to use our 'menubar' for the actual menubar stuff,
     but our menu_hbox for the widget being docked. */
  {
    GtkWidget *dock_item;
    GnomeDockItemBehavior behavior;

    behavior = (GNOME_DOCK_ITEM_BEH_EXCLUSIVE
		| GNOME_DOCK_ITEM_BEH_NEVER_VERTICAL);
	
    if (!gnome_preferences_get_menubar_detachable())
      behavior |= GNOME_DOCK_ITEM_BEH_LOCKED;

    dock_item = gnome_dock_item_new (GNOME_APP_MENUBAR_NAME,
				     behavior);
    gtk_container_add (GTK_CONTAINER (dock_item), GTK_WIDGET (menu_hbox));

    app->menubar = GTK_WIDGET (menubar);

    /* To have menubar relief agree with the toolbar (and have the relief outside of
     * smaller handles), substitute the dock item's relief for the menubar's relief,
     * but don't change the size of the menubar in the process. */
    gtk_menu_bar_set_shadow_type (GTK_MENU_BAR (app->menubar), GTK_SHADOW_NONE);
    if (gnome_preferences_get_menubar_relief ()) {
      guint border_width;
		
      gtk_container_set_border_width (GTK_CONTAINER (dock_item), 2);
      border_width = GTK_CONTAINER (app->menubar)->border_width;
      if (border_width >= 2)
	border_width -= 2;
      gtk_container_set_border_width (GTK_CONTAINER (app->menubar),
				      border_width);
    } else
      gnome_dock_item_set_shadow_type (GNOME_DOCK_ITEM (dock_item), GTK_SHADOW_NONE);

    if (app->layout != NULL)
      gnome_dock_layout_add_item (app->layout,
				  GNOME_DOCK_ITEM (dock_item),
				  GNOME_DOCK_TOP,
				  0, 0, 0);
    else
      gnome_dock_add_item(GNOME_DOCK(app->dock),
			  GNOME_DOCK_ITEM (dock_item),
			  GNOME_DOCK_TOP,
			  0, 0, 0, TRUE);

    gtk_widget_show (menubar);
    gtk_widget_show (dock_item);
  }
  gtk_widget_show (menu_hbox);

  gnome_app_set_statusbar(app, (statusbar = gtk_statusbar_new()));
  window->statusbar_ctx = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar), "IhateGtkStatusbar");

  gnome_app_install_menu_hints(app, main_menu); /* This needs a statusbar in order to work */

  wtmp = gnome_stock_pixmap_widget(GTK_WIDGET(window), GNOME_STOCK_PIXMAP_BACK);
  window->btn_back = gtk_button_new();
  gtk_button_set_relief(GTK_BUTTON(window->btn_back), GTK_RELIEF_NONE);
  gtk_widget_set_sensitive(window->btn_back, FALSE);
  gtk_container_add(GTK_CONTAINER(window->btn_back), wtmp);
  gtk_signal_connect(GTK_OBJECT(window->btn_back), "clicked", nautilus_window_back, window);

  wtmp = gnome_stock_pixmap_widget(GTK_WIDGET(window), GNOME_STOCK_PIXMAP_FORWARD);
  window->btn_fwd = gtk_button_new();
  gtk_button_set_relief(GTK_BUTTON(window->btn_fwd), GTK_RELIEF_NONE);
  gtk_widget_set_sensitive(window->btn_fwd, FALSE);
  gtk_container_add(GTK_CONTAINER(window->btn_fwd), wtmp);
  gtk_signal_connect(GTK_OBJECT(window->btn_fwd), "clicked", nautilus_window_fwd, window);

  gtk_box_pack_start(GTK_BOX(menu_hbox), window->btn_back, FALSE, FALSE, GNOME_PAD_SMALL);
  gtk_box_pack_start(GTK_BOX(menu_hbox), window->btn_fwd, FALSE, FALSE, GNOME_PAD_SMALL);
  gtk_widget_show_all(window->btn_back);
  gtk_widget_show_all(window->btn_fwd);

  gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);

#ifdef CONTENTS_AS_HBOX
  window->content_hbox = gtk_hbox_new(FALSE, GNOME_PAD_SMALL);
#else
  window->content_hbox = gtk_hpaned_new();
#endif
  gnome_app_set_contents(app, window->content_hbox);

  window->meta_notebook = gtk_notebook_new();
  gtk_widget_show(window->meta_notebook);
#ifdef CONTENTS_AS_HBOX
  gtk_box_pack_end(GTK_BOX(window->content_hbox), window->meta_notebook, FALSE, FALSE, GNOME_PAD);
#else
  gtk_paned_pack2(GTK_PANED(window->content_hbox), window->meta_notebook, TRUE, TRUE);
#endif
  gtk_widget_show_all(window->content_hbox);


  window->ent_url = explorer_location_bar_new();
  gtk_signal_connect(GTK_OBJECT(window->ent_url), "location_changed",
                     nautilus_window_goto_url_cb, window);
  gnome_app_add_docked(app, window->ent_url, "url-entry",
                       GNOME_DOCK_ITEM_BEH_LOCKED|GNOME_DOCK_ITEM_BEH_EXCLUSIVE|GNOME_DOCK_ITEM_BEH_NEVER_VERTICAL,
                       GNOME_DOCK_TOP, 1, 0, 0);
  gtk_widget_show(window->ent_url);

  /* CORBA stuff */
  window->ntl_viewwindow = impl_Nautilus_ViewWindow__create(window);
  window->uih = gnome_ui_handler_new();
  gnome_ui_handler_set_app(window->uih, app);
  gnome_object_add_interface(GNOME_OBJECT(window->uih), window->ntl_viewwindow);
}

static void
nautilus_window_set_arg (GtkObject      *object,
			  GtkArg         *arg,
			  guint	      arg_id)
{
  GnomeApp *app = (GnomeApp *) object;
  char *old_app_name;
  NautilusView *new_cv;
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
    new_cv = NAUTILUS_VIEW(GTK_VALUE_OBJECT(*arg));
    if(window->content_view)
      {
	gtk_widget_ref(GTK_WIDGET(window->content_view));
	gtk_container_remove(GTK_CONTAINER(window->content_hbox), GTK_WIDGET(window->content_view));
#ifdef CONTENTS_AS_HBOX
	gtk_box_pack_start(GTK_BOX(window->content_hbox), GTK_WIDGET(new_cv), TRUE, TRUE, GNOME_PAD);
#else
	gtk_paned_pack1(GTK_PANED(window->content_hbox), GTK_WIDGET(new_cv), TRUE, FALSE);
#endif
	gtk_widget_unref(GTK_WIDGET(window->content_view));
      }
    else
#ifdef CONTENTS_AS_HBOX
      gtk_box_pack_start(GTK_BOX(window->content_hbox), GTK_WIDGET(new_cv), TRUE, TRUE, GNOME_PAD);
#else
      gtk_paned_pack1(GTK_PANED(window->content_hbox), new_cv, TRUE, FALSE);
#endif

    gtk_widget_queue_resize(window->content_hbox);
    window->content_view = new_cv;
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

static void
nautilus_window_init (NautilusWindow *window)
{
}

static void nautilus_window_destroy (NautilusWindow *window)
{
  g_slist_free(window->meta_views);
  CORBA_free(window->ni);
  CORBA_free(window->si);
  g_slist_foreach(window->uris_prev, (GFunc)g_free, NULL);
  g_slist_foreach(window->uris_next, (GFunc)g_free, NULL);
  g_slist_free(window->uris_prev);
  g_slist_free(window->uris_next);

  if(window->statusbar_clear_id)
    g_source_remove(window->statusbar_clear_id);
}

GtkWidget *
nautilus_window_new(const char *app_id)
{
  return GTK_WIDGET (gtk_object_new (nautilus_window_get_type(), "app_id", app_id, NULL));
}

static gboolean
nautilus_window_send_show_properties(GtkWidget *dockitem, GdkEventButton *event, NautilusView *meta_view)
{
  if(event->button != 3)
    return FALSE;

  gtk_signal_emit_stop_by_name(GTK_OBJECT(dockitem), "button_press_event");

  gtk_signal_emit_by_name(GTK_OBJECT(meta_view), "show_properties");

  return TRUE;
}

void
nautilus_window_set_content_view(NautilusWindow *window, NautilusView *content_view)
{
  gtk_object_set(GTK_OBJECT(window), "content_view", content_view, NULL);
  gtk_widget_show(GTK_WIDGET(content_view));
}

void
nautilus_window_add_meta_view(NautilusWindow *window, NautilusView *meta_view)
{
  GtkWidget *label;
  const char *desc;
  char cbuf[32];

  g_return_if_fail(!g_slist_find(window->meta_views, meta_view));
  g_return_if_fail(NAUTILUS_IS_META_VIEW(meta_view));

  desc = nautilus_meta_view_get_label(NAUTILUS_META_VIEW(meta_view));
  if(!desc)
    {
      desc = cbuf;
      g_snprintf(cbuf, sizeof(cbuf), "%p", meta_view);
    }
  label = gtk_label_new(desc);
  gtk_widget_show(label);
  gtk_signal_connect(GTK_OBJECT(label), "button_press_event",
		     GTK_SIGNAL_FUNC(nautilus_window_send_show_properties), meta_view);
  gtk_notebook_prepend_page(GTK_NOTEBOOK(window->meta_notebook), GTK_WIDGET(meta_view), label);
  gtk_widget_show(GTK_WIDGET(meta_view));

  window->meta_views = g_slist_prepend(window->meta_views, meta_view);
}

void
nautilus_window_remove_meta_view(NautilusWindow *window, NautilusView *meta_view)
{
  gint pagenum;

  g_return_if_fail(g_slist_find(window->meta_views, meta_view));

  window->meta_views = g_slist_remove(window->meta_views, meta_view);

  pagenum = gtk_notebook_page_num(GTK_NOTEBOOK(window->meta_notebook), GTK_WIDGET(meta_view));

  g_return_if_fail(pagenum >= 0);

  gtk_notebook_remove_page(GTK_NOTEBOOK(window->meta_notebook), pagenum);
}

static void
nautilus_window_back (GtkWidget *btn, NautilusWindow *window)
{
  Nautilus_NavigationRequestInfo nri;

  g_assert(window->uris_prev);

  memset(&nri, 0, sizeof(nri));
  nri.requested_uri = window->uris_prev->data;
  nri.new_window_default = nri.new_window_suggested = nri.new_window_enforced = Nautilus_V_FALSE;

  nautilus_window_change_location(window, &nri, NULL, TRUE);
}

static void
nautilus_window_fwd (GtkWidget *btn, NautilusWindow *window)
{
  Nautilus_NavigationRequestInfo nri;

  g_assert(window->uris_next);

  memset(&nri, 0, sizeof(nri));
  nri.requested_uri = window->uris_next->data;
  nri.new_window_default = nri.new_window_suggested = nri.new_window_enforced = Nautilus_V_FALSE;
  nautilus_window_change_location(window, &nri, NULL, FALSE);
}
