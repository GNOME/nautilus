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


typedef struct {
  POA_Nautilus_ViewWindow servant;
  gpointer gnome_object;

  NautilusWindow *window;
} impl_POA_Nautilus_ViewWindow;

static POA_Nautilus_ViewWindow__epv impl_Nautilus_ViewWindow_epv =
{
  NULL
};

static PortableServer_ServantBase__epv base_epv = { NULL};

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
static void nautilus_window_goto_url (GtkWidget *widget,
                                      const char *url,
                                      GtkWidget *window);
static void nautilus_window_real_request_location_change (NautilusWindow *window,
							  Nautilus_NavigationRequestInfo *loc,
							  GtkWidget *requesting_view);
static void nautilus_window_real_request_selection_change(NautilusWindow *window,
							  Nautilus_SelectionRequestInfo *loc,
							  GtkWidget *requesting_view);
static void nautilus_window_real_request_status_change(NautilusWindow *window,
                                                       Nautilus_StatusRequestInfo *loc,
                                                       GtkWidget *requesting_view);

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
	(GtkObjectInitFunc) nautilus_window_init
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

static void
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

static void
nautilus_window_goto_url (GtkWidget *widget,
                          const char *url,
                          GtkWidget *window)
{
  Nautilus_NavigationRequestInfo navinfo;

  memset(&navinfo, 0, sizeof(navinfo));
  navinfo.requested_uri = (char *)url;
  navinfo.new_window_default = navinfo.new_window_suggested = Nautilus_V_FALSE;
  navinfo.new_window_enforced = Nautilus_V_UNKNOWN;

  nautilus_window_request_location_change(NAUTILUS_WINDOW(window), &navinfo,
                                          NULL);
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
                     nautilus_window_goto_url, window);
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
  GtkWidget *new_cv;
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
    new_cv = GTK_WIDGET(GTK_VALUE_OBJECT(*arg));
    if(window->content_view)
      {
	gtk_widget_ref(GTK_WIDGET(window->content_view));
	gtk_container_remove(GTK_CONTAINER(window->content_hbox), GTK_WIDGET(window->content_view));
#ifdef CONTENTS_AS_HBOX
	gtk_box_pack_start(GTK_BOX(window->content_hbox), new_cv, TRUE, TRUE, GNOME_PAD);
#else
	gtk_paned_pack1(GTK_PANED(window->content_hbox), new_cv, TRUE, FALSE);
#endif
	gtk_widget_unref(GTK_WIDGET(window->content_view));
      }
    else
#ifdef CONTENTS_AS_HBOX
      gtk_box_pack_start(GTK_BOX(window->content_hbox), new_cv, TRUE, TRUE, GNOME_PAD);
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
  g_free(window->current_uri);
  g_free(window->actual_current_uri);
  g_free(window->current_content_type);
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

void
nautilus_window_set_content_view(NautilusWindow *window, NautilusView *content_view)
{
  gtk_object_set(GTK_OBJECT(window), "content_view", content_view, NULL);
  gtk_widget_show(GTK_WIDGET(content_view));
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

void
nautilus_window_request_status_change(NautilusWindow *window,
                                      Nautilus_StatusRequestInfo *loc,
                                      GtkWidget *requesting_view)
{
  NautilusWindowClass *klass;
  GtkObject *obj;

  obj = GTK_OBJECT(window);

  klass = NAUTILUS_WINDOW_CLASS(obj->klass);
  gtk_signal_emit(obj, klass->window_signals[2], loc, requesting_view);
}

void
nautilus_window_request_selection_change(NautilusWindow *window,
					 Nautilus_SelectionRequestInfo *loc,
					 GtkWidget *requesting_view)
{
  NautilusWindowClass *klass;
  GtkObject *obj;

  obj = GTK_OBJECT(window);

  klass = NAUTILUS_WINDOW_CLASS(obj->klass);
  gtk_signal_emit(obj, klass->window_signals[1], loc, requesting_view);
}

void
nautilus_window_request_location_change(NautilusWindow *window,
					Nautilus_NavigationRequestInfo *loc,
					GtkWidget *requesting_view)
{
  NautilusWindowClass *klass;
  GtkObject *obj;

  obj = GTK_OBJECT(window);

  klass = NAUTILUS_WINDOW_CLASS(obj->klass);
  gtk_signal_emit(obj, klass->window_signals[0], loc, requesting_view);
}

static void
nautilus_window_change_location(NautilusWindow *window,
				Nautilus_NavigationRequestInfo *loc,
				GtkWidget *requesting_view,
				gboolean is_back)
{
  guint signum;
  GSList *cur;
  GSList *discard_views, *keep_views, *notfound_views;

  NautilusNavigationInfo loci_spot, *loci;

  loci = nautilus_navinfo_new(&loci_spot, loc, window->current_uri, window->actual_current_uri, window->current_content_type,
			      requesting_view);

  if(!loci)
    {
      char cbuf[1024];
      g_snprintf(cbuf, sizeof(cbuf), _("Cannot load %s"), loc->requested_uri);
      nautilus_window_set_status(window, cbuf);
      return;
    }

  /* Update history */
  if(is_back)
    {
      window->uris_next = g_slist_prepend(window->uris_next, window->uris_prev->data);
      window->uris_prev = g_slist_remove(window->uris_prev, window->uris_prev->data);
    }
  else
    {
      char *append_val;
      if(window->uris_next && !strcmp(loc->requested_uri, window->uris_next->data))
	{
	  append_val = window->uris_next->data;
	  window->uris_next = g_slist_remove(window->uris_next, window->uris_next->data);
	}
      else
	{
	  append_val = g_strdup(loc->requested_uri);
	  g_slist_foreach(window->uris_next, (GFunc)g_free, NULL);
	  g_slist_free(window->uris_next); window->uris_next = NULL;
	}
      if(append_val)
        window->uris_prev = g_slist_prepend(window->uris_prev, append_val);
    }
  gtk_widget_set_sensitive(window->btn_back, window->uris_prev?TRUE:FALSE);
  gtk_widget_set_sensitive(window->btn_fwd, window->uris_next?TRUE:FALSE);

  explorer_location_bar_set_uri_string(EXPLORER_LOCATION_BAR(window->ent_url),
                                       loci->navinfo.requested_uri);
  signum = gtk_signal_lookup("notify_location_change", nautilus_view_get_type());

  /* If we need to load a different IID, do that before sending the location change request */
  if(!window->content_view || strcmp(NAUTILUS_VIEW(window->content_view)->iid, loci->content_iid)) {
    NautilusView *new_view;

    if(requesting_view == window->content_view)
      requesting_view = NULL;

    new_view = NAUTILUS_VIEW(gtk_widget_new(nautilus_content_view_get_type(), "main_window", window, NULL));
    nautilus_view_load_client(new_view, loci->content_iid);
    nautilus_window_set_content_view(window, new_view);
  }

  loci->navinfo.content_view = NAUTILUS_VIEW(window->content_view)->view_client;

  loci->navinfo.self_originated = (requesting_view == window->content_view);
  gtk_signal_emit(GTK_OBJECT(window->content_view), signum, loci);

  notfound_views = keep_views = discard_views = NULL;
  for(cur = window->meta_views; cur; cur = cur->next)
    {
      NautilusView *view = cur->data;

      if(g_slist_find_custom(loci->meta_iids, view->iid, (GCompareFunc)strcmp))
	{
          loci->navinfo.self_originated = (requesting_view == ((GtkWidget *)view));
          gtk_signal_emit(GTK_OBJECT(view), signum, loci);
	}
      else
	{
	  if(((GtkWidget *)view) == requesting_view)
	    requesting_view = NULL;
	  discard_views = g_slist_prepend(discard_views, view);
	}
    }
  loci->navinfo.self_originated = FALSE;
  for(cur = loci->meta_iids; cur; cur = cur->next)
    {
      GSList *curview;
      for(curview = window->meta_views; curview; curview = curview->next)
	{
	  if(!strcmp(NAUTILUS_VIEW(curview->data)->iid, cur->data))
	    break;
	}
      if(!curview)
	{
	  NautilusView *view;
	  view = NAUTILUS_VIEW(gtk_widget_new(nautilus_meta_view_get_type(), "main_window", window, NULL));
	  nautilus_view_load_client(view, cur->data);
	  nautilus_window_add_meta_view(window, view);
	  gtk_signal_emit(GTK_OBJECT(view), signum, loci, window->content_view);
	}
    }
  for(cur = discard_views; cur; cur = cur->next)
    nautilus_window_remove_meta_view(window, cur->data);
  g_slist_free(discard_views);

  g_free(window->current_content_type);
  window->current_content_type = g_strdup(loci->navinfo.content_type);
  g_free(window->current_uri);
  window->current_uri = g_strdup(loci->navinfo.requested_uri);
  g_free(window->actual_current_uri);
  window->actual_current_uri = g_strdup(loci->navinfo.actual_uri);

  nautilus_navinfo_free(loci);
}

static void nautilus_window_real_request_selection_change(NautilusWindow *window,
							  Nautilus_SelectionRequestInfo *loc,
							  GtkWidget *requesting_view)
{
  GSList *cur;
  guint signum;
  Nautilus_SelectionInfo selinfo;

  signum = gtk_signal_lookup("notify_selection_change", nautilus_view_get_type());

  selinfo.selected_uri = loc->selected_uri;
  selinfo.content_view = NAUTILUS_VIEW(window->content_view)->view_client;

  selinfo.self_originated = (((GtkWidget *)window->content_view) == requesting_view);
    gtk_signal_emit(GTK_OBJECT(window->content_view), signum, &selinfo);

  for(cur = window->meta_views; cur; cur = cur->next)
    {
      selinfo.self_originated = (cur->data == requesting_view);
      gtk_signal_emit(GTK_OBJECT(window->content_view), signum, &selinfo);
    }
}

static void
nautilus_window_real_request_status_change(NautilusWindow *window,
                                           Nautilus_StatusRequestInfo *loc,
                                           GtkWidget *requesting_view)
{
  nautilus_window_set_status(window, loc->status_string);
}

static void
nautilus_window_real_request_location_change (NautilusWindow *window,
					      Nautilus_NavigationRequestInfo *loc,
					      GtkWidget *requesting_view)
{
  nautilus_window_change_location(window, loc, requesting_view, FALSE);
}

void
nautilus_window_save_state(NautilusWindow *window, const char *config_path)
{
  GSList *cur;
  int n;
  guint signum;
  char cbuf[1024];

  gnome_config_push_prefix(config_path);
  if(window->content_view)
    {
      gnome_config_set_string("content_view_type", NAUTILUS_VIEW(window->content_view)->iid);
      g_snprintf(cbuf, sizeof(cbuf), "%s/Content_View/", config_path);

      gnome_config_push_prefix(cbuf);

      gtk_signal_emit(GTK_OBJECT(window->content_view), signum, cbuf);

      gnome_config_pop_prefix();
    }
  else
    gnome_config_set_string("content_view_type", "NONE");


  n = g_slist_length(window->meta_views);
  signum = gtk_signal_lookup("save_state", nautilus_view_get_type());
  for(n = 0, cur = window->meta_views; cur; cur = cur->next, n++)
    {
      if(!NAUTILUS_VIEW(cur->data)->iid)
	{
	  continue;
	  n--;
	}

      g_snprintf(cbuf, sizeof(cbuf), "meta_view_%d_type=0", n);

      gnome_config_set_string(cbuf, NAUTILUS_VIEW(cur->data)->iid);

      g_snprintf(cbuf, sizeof(cbuf), "%s/Meta_View_%d/", config_path, n);

      gnome_config_push_prefix(cbuf);

      gtk_signal_emit(GTK_OBJECT(cur->data), signum, cbuf);

      gnome_config_pop_prefix();
    }
  gnome_config_set_int("num_meta_views", n);

  gnome_config_pop_prefix();
}

void
nautilus_window_set_initial_state(NautilusWindow *window)
{
  nautilus_window_goto_url(NULL, "file://localhost/", (GtkWidget *)window);

#if 0
  GSList *cur;
  GtkRequisition sreq;
  GdkGeometry geo;
  NautilusView *view;

  /* Remove old stuff */
  for(cur = window->meta_views; cur; cur = cur->next)
    nautilus_window_remove_meta_view(window, NAUTILUS_VIEW(cur->data));

  view = NAUTILUS_VIEW(gtk_widget_new(nautilus_content_view_get_type(), "main_window", window, NULL));
  nautilus_view_load_client(view, "control:clock");
  nautilus_window_set_content_view(window, view);

  view = NAUTILUS_VIEW(gtk_widget_new(nautilus_meta_view_get_type(), "main_window", window, NULL));
  nautilus_view_load_client(view, "control:clock");
  nautilus_window_add_meta_view(window, view);

  gtk_widget_size_request(GTK_WIDGET(window), &sreq);
  gtk_widget_size_request(GTK_WIDGET(window->content_hbox), &sreq);
  gtk_widget_size_request(GTK_WIDGET(window->meta_notebook), &sreq);
  geo.min_width = window->meta_notebook->requisition.width + MAX(400, GTK_WIDGET(window->content_view)->requisition.width);
  geo.min_height = window->meta_notebook->requisition.height + MAX(200, GTK_WIDGET(window->content_view)->requisition.height);
  geo.base_width = geo.min_width;
  geo.base_height = geo.min_height;
  gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geo,
				GDK_HINT_BASE_SIZE|GDK_HINT_MIN_SIZE);
  gtk_window_set_default_size(GTK_WINDOW(window), geo.min_width, geo.min_height);

#ifndef CONTENTS_AS_HBOX
  gtk_paned_set_position(GTK_PANED(window->content_hbox), MAX(400, GTK_WIDGET(window->content_view)->requisition.width));
#endif
#endif
}

void
nautilus_window_load_state(NautilusWindow *window, const char *config_path)
{
  char *vtype;
  GSList *cur;
  int i, n;
  guint signum;
  char cbuf[1024];

  /* Remove old stuff */
  for(cur = window->meta_views; cur; cur = cur->next)
    nautilus_window_remove_meta_view(window, NAUTILUS_VIEW(cur->data));
  nautilus_window_set_content_view(window, NULL);

  /* Load new stuff */
  gnome_config_push_prefix(config_path);

  vtype = gnome_config_get_string("content_view_type=NONE");
  signum = gtk_signal_lookup("load_state", nautilus_view_get_type());

  if(vtype && strcmp(vtype, "NONE")) /* Create new content view */
    {
      GtkWidget *w;

      w = gtk_widget_new(nautilus_content_view_get_type(), "main_window", window, NULL);
      nautilus_view_load_client(NAUTILUS_VIEW(w), vtype);
      nautilus_window_set_content_view(window, NAUTILUS_VIEW(w));

      g_snprintf(cbuf, sizeof(cbuf), "%s/Content_View/", config_path);
      gnome_config_push_prefix(cbuf);
      gtk_signal_emit(GTK_OBJECT(w), signum, cbuf);
      gnome_config_pop_prefix();

      gtk_widget_show(w);
    }
  g_free(vtype);

  n = gnome_config_get_int("num_meta_views=0");
  for(i = 0; i < n; n++)
    {
      GtkWidget *nvw;

      g_snprintf(cbuf, sizeof(cbuf), "%s/meta_view_%d_type=0", config_path, i);
      vtype = gnome_config_get_string(cbuf);

      nvw = gtk_widget_new(nautilus_meta_view_get_type(), "main_window", window, NULL);
      nautilus_view_load_client(NAUTILUS_VIEW(nvw), vtype);

      g_snprintf(cbuf, sizeof(cbuf), "%s/Meta_View_%d/", config_path, i);

      gnome_config_push_prefix(cbuf);
      gtk_signal_emit(GTK_OBJECT(cur->data), signum, cbuf);
      gnome_config_pop_prefix();

      nautilus_window_add_meta_view(window, NAUTILUS_VIEW(nvw));
    }

  gnome_config_pop_prefix();
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
