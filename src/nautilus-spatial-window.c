#include "config.h"
#include <gnome.h>
#include "nautilus.h"

static void nautilus_window_class_init (NautilusWindowClass *klass);
static void nautilus_window_init (NautilusWindow *window);
static void nautilus_window_set_arg (GtkObject      *object,
				      GtkArg         *arg,
				      guint	      arg_id);
static void nautilus_window_get_arg (GtkObject      *object,
				      GtkArg         *arg,
				      guint	      arg_id);
static void nautilus_window_close (GtkWidget *widget,
				    GtkWidget *window);
static void nautilus_window_real_request_location_change (NautilusWindow *window,
							  NautilusLocationReference loc,
							  GtkWidget *requesting_view);

GtkType
nautilus_window_get_type(void)
{
  static guint window_type = 0;

  if (!window_type)
    {
      GtkTypeInfo window_info = {
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

typedef void (*GtkSignal_NONE__STRING_OBJECT) (GtkObject * object,
					       const char *arg1,
					       GtkObject *arg2,
					       gpointer user_data);
static void 
gtk_marshal_NONE__STRING_OBJECT (GtkObject * object,
			       GtkSignalFunc func,
			       gpointer func_data,
			       GtkArg * args)
{
  GtkSignal_NONE__STRING_OBJECT rfunc;
  rfunc = (GtkSignal_NONE__STRING_OBJECT) func;
  (*rfunc) (object,
	    GTK_VALUE_STRING (args[0]),
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
  object_class->get_arg = nautilus_window_get_arg;
  object_class->set_arg = nautilus_window_set_arg;

  widget_class = (GtkWidgetClass*) klass;
  klass->parent_class = gtk_type_class (gtk_type_parent (object_class->type));

  klass->request_location_change = nautilus_window_real_request_location_change;

  i = 0;
  klass->window_signals[i++] = gtk_signal_new("request_location_change",
					      GTK_RUN_LAST,
					      object_class->type,
					      GTK_SIGNAL_OFFSET (NautilusWindowClass, request_location_change),
					      gtk_marshal_NONE__STRING_OBJECT,
					      GTK_TYPE_NONE, 2, GTK_TYPE_STRING, GTK_TYPE_OBJECT);
  gtk_object_class_add_signals (object_class, klass->window_signals, i);

  gtk_object_add_arg_type ("NautilusWindow::app_id",
			   GTK_TYPE_STRING,
			   GTK_ARG_READWRITE|GTK_ARG_CONSTRUCT,
			   ARG_APP_ID);
  gtk_object_add_arg_type ("NautilusWindow::content_view",
			   GTK_TYPE_OBJECT,
			   GTK_ARG_READWRITE,
			   ARG_CONTENT_VIEW);
}

static void
nautilus_window_constructed(NautilusWindow *window)
{
  GnomeApp *app;
  GnomeUIInfo dummy_menu[] = {
    GNOMEUIINFO_END
  };
  GnomeUIInfo main_menu[] = {
    GNOMEUIINFO_MENU_CLOSE_ITEM(nautilus_window_close, NULL),
    GNOMEUIINFO_SUBTREE_STOCK(N_("Actions"), dummy_menu, GNOME_STOCK_MENU_JUMP_TO),
    GNOMEUIINFO_END
  };
#if 0
  GnomeUIInfo main_menu[] = {
    GNOMEUIINFO_SUBTREE("Main", ops_menu),
    GNOMEUIINFO_END
  };
#endif
  GtkWidget *menu_hbox, *menubar, *wtmp;
  GtkAccelGroup *ag;

  app = GNOME_APP(window);

  ag = gtk_object_get_data(GTK_OBJECT(app), "GtkAccelGroup");
  if (ag && !g_slist_find(gtk_accel_groups_from_object (GTK_OBJECT (app)), ag))
    gtk_window_add_accel_group(GTK_WINDOW(app), ag);

  menu_hbox = gtk_hbox_new(FALSE, GNOME_PAD);
  menubar = gtk_menu_bar_new();
  gtk_box_pack_end(GTK_BOX(menu_hbox), menubar, TRUE, TRUE, GNOME_PAD_BIG);
  gnome_app_fill_menu_with_data(GTK_MENU_SHELL(menubar), main_menu, ag, TRUE, 0, window);

  /* A hacked-up version of gnome_app_set_menu() */
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

  gnome_app_set_statusbar(app, gtk_statusbar_new());
  gnome_app_install_menu_hints(app, main_menu); /* This needs a statusbar in order to work */

  wtmp = gnome_stock_pixmap_widget(GTK_WIDGET(window), GNOME_STOCK_PIXMAP_BACK);
  window->btn_back = gtk_button_new();
  gtk_button_set_relief(GTK_BUTTON(window->btn_back), GTK_RELIEF_NONE);
  gtk_container_add(GTK_CONTAINER(window->btn_back), wtmp);

  wtmp = gnome_stock_pixmap_widget(GTK_WIDGET(window), GNOME_STOCK_PIXMAP_FORWARD);
  window->btn_fwd = gtk_button_new();
  gtk_button_set_relief(GTK_BUTTON(window->btn_fwd), GTK_RELIEF_NONE);
  gtk_container_add(GTK_CONTAINER(window->btn_fwd), wtmp);

  gtk_box_pack_start(GTK_BOX(menu_hbox), window->btn_back, FALSE, FALSE, GNOME_PAD_SMALL);
  gtk_box_pack_start(GTK_BOX(menu_hbox), window->btn_fwd, FALSE, FALSE, GNOME_PAD_SMALL);
  gtk_widget_show_all(window->btn_back);
  gtk_widget_show_all(window->btn_fwd);

  gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);

  window->content_hbox = gtk_hpaned_new();
  gtk_widget_show(window->content_hbox);
  gnome_app_set_contents(app, window->content_hbox);

  window->nav_notebook = gtk_notebook_new();
  gtk_widget_show(window->nav_notebook);
#if 0
  gtk_box_pack_end(GTK_BOX(window->content_hbox), window->nav_notebook, FALSE, FALSE, GNOME_PAD);
#else
  gtk_paned_pack2(GTK_PANED(window->content_hbox), window->nav_notebook, TRUE, TRUE);
#endif
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
#if 0
	gtk_box_pack_start(GTK_BOX(window->content_hbox), new_cv, TRUE, TRUE, GNOME_PAD);
#else
	gtk_paned_pack1(GTK_PANED(window->content_hbox), new_cv, TRUE, FALSE);
#endif
	gtk_widget_unref(GTK_WIDGET(window->content_view));
      }
    else
#if 0
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

GtkWidget *
nautilus_window_new(const char *app_id)
{
  return GTK_WIDGET (gtk_object_new (nautilus_window_get_type(), "app_id", app_id, NULL));
}

static void
nautilus_window_close (GtkWidget *widget,
			GtkWidget *window)
{
  gtk_widget_destroy(window);
}

void
nautilus_window_set_content_view(NautilusWindow *window, GtkWidget *content_view)
{
  gtk_object_set(GTK_OBJECT(window), "content_view", content_view, NULL);
  gtk_widget_show(GTK_WIDGET(content_view));
}

static gboolean
nautilus_window_send_show_properties(GtkWidget *dockitem, GdkEventButton *event, NautilusView *nav_view)
{
  if(event->button != 3)
    return FALSE;

  gtk_signal_emit_stop_by_name(GTK_OBJECT(dockitem), "button_press_event");

  gtk_signal_emit_by_name(GTK_OBJECT(nav_view), "show_properties");

  return TRUE;
}

void
nautilus_window_add_meta_view(NautilusWindow *window, NautilusView *nav_view)
{
  GtkWidget *label;

  g_return_if_fail(!g_slist_find(window->nav_views, nav_view));
  g_return_if_fail(NAUTILUS_IS_META_VIEW(nav_view));

  label = gtk_label_new(nautilus_meta_view_get_description(NAUTILUS_META_VIEW(nav_view)));
  gtk_widget_show(label);
  gtk_notebook_prepend_page(GTK_NOTEBOOK(window->nav_notebook), GTK_WIDGET(nav_view), label);
  gtk_widget_show(GTK_WIDGET(nav_view));

  window->nav_views = g_slist_prepend(window->nav_views, nav_view);
}

void
nautilus_window_remove_meta_view(NautilusWindow *window, NautilusView *nav_view)
{
  gint pagenum;

  g_return_if_fail(g_slist_find(window->nav_views, nav_view));

  window->nav_views = g_slist_remove(window->nav_views, nav_view);

  pagenum = gtk_notebook_page_num(GTK_NOTEBOOK(window->nav_notebook), GTK_WIDGET(nav_view));

  g_return_if_fail(pagenum >= 0);

  gtk_notebook_remove_page(GTK_NOTEBOOK(window->nav_notebook), pagenum);
}

void
nautilus_window_request_location_change(NautilusWindow *window,
					NautilusLocationReference loc,
					GtkWidget *requesting_view)
{
  NautilusWindowClass *klass;
  GtkObject *obj;

  obj = GTK_OBJECT(window);

  klass = NAUTILUS_WINDOW_CLASS(obj->klass);
  gtk_signal_emit(obj, klass->window_signals[0], loc, requesting_view);
}

static void
nautilus_window_real_request_location_change (NautilusWindow *window,
					      NautilusLocationReference loc,
					      GtkWidget *requesting_view)
{
  guint signum;
  GSList *cur;
  NautilusNavigationInfo loci_spot, *loci;

  memset(&loci_spot, 0, sizeof(loci_spot));
  loci = &loci_spot;
  loci->requested_uri = loci->actual_uri = loc;
  loci->referring_uri = window->current_uri;
  loci->actual_referring_uri = window->actual_current_uri;

  signum = gtk_signal_lookup("notify_location_change", nautilus_view_get_type());

  for(cur = window->nav_views; cur; cur = cur->next)
    gtk_signal_emit(GTK_OBJECT(cur->data), signum, loci, window->content_view, requesting_view);

  gtk_signal_emit(GTK_OBJECT(window->content_view), signum, loci, window->content_view, requesting_view);

  g_free(window->current_uri);
  window->current_uri = loci->requested_uri;
  g_free(window->actual_current_uri);
  window->actual_current_uri = loci->actual_uri;
}

void
nautilus_window_save_state(NautilusWindow *window, const char *config_path)
{
#if 0
  GSList *cur;
  int n;
  guint signum;
  char cbuf[1024];

  gnome_config_push_prefix(config_path);
  if(window->content_view)
    {
      gnome_config_set_string("content_view_type", gtk_type_name(GTK_OBJECT_TYPE(window->content_view)));
      g_snprintf(cbuf, sizeof(cbuf), "%s/Content_View/", config_path);

      gnome_config_push_prefix(cbuf);

      gtk_signal_emit(GTK_OBJECT(window->content_view), signum, cbuf);

      gnome_config_pop_prefix();
    }
  else
    gnome_config_set_string("content_view_type", "NONE");


  n = g_slist_length(window->nav_views);
  gnome_config_set_int("num_nav_views", n);
  signum = gtk_signal_lookup("save_state", nautilus_view_get_type());
  for(n = 0, cur = window->nav_views; cur; cur = cur->next, n++)
    {
      g_snprintf(cbuf, sizeof(cbuf), "nav_view_%d_type=0", n);

      gnome_config_set_string(cbuf, gtk_type_name(GTK_OBJECT_TYPE(cur->data)));

      g_snprintf(cbuf, sizeof(cbuf), "%s/Nav_View_%d/", config_path, n);

      gnome_config_push_prefix(cbuf);

      gtk_signal_emit(GTK_OBJECT(cur->data), signum, cbuf);

      gnome_config_pop_prefix();
    }

  gnome_config_pop_prefix();
#endif
}

void
nautilus_window_set_initial_state(NautilusWindow *window)
{
#if 0
  GSList *cur;
  GtkRequisition sreq;
  GdkGeometry geo;

  /* Remove old stuff */
  for(cur = window->nav_views; cur; cur = cur->next)
    nautilus_window_remove_meta_view(window, NAUTILUS_VIEW(cur->data));

  nautilus_window_set_content_view(window,
				    NAUTILUS_VIEW(gtk_widget_new(nautilus_content_gtkhtml_get_type(),
								  "main_window", window, NULL))
				    );

  nautilus_window_add_meta_view(window, NAUTILUS_VIEW(gtk_widget_new(nautilus_meta_location_get_type(),
									     "main_window", window, NULL))
				       );
  nautilus_window_add_meta_view(window, NAUTILUS_VIEW(gtk_widget_new(nautilus_meta_history_get_type(),
									     "main_window", window, NULL))
				       );

  nautilus_window_add_meta_view(window, NAUTILUS_VIEW(gtk_widget_new(nautilus_meta_tree_get_type(),
									     "main_window", window, NULL))
				       );

  gtk_widget_size_request(GTK_WIDGET(window), &sreq);
  gtk_widget_size_request(GTK_WIDGET(window->content_hbox), &sreq);
  gtk_widget_size_request(GTK_WIDGET(window->nav_notebook), &sreq);
  geo.min_width = window->nav_notebook->requisition.width + MAX(400, GTK_WIDGET(window->content_view)->requisition.width);
  geo.min_height = window->nav_notebook->requisition.height + MAX(200, GTK_WIDGET(window->content_view)->requisition.height);
  geo.base_width = geo.min_width;
  geo.base_height = geo.min_height;
  gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geo,
				GDK_HINT_BASE_SIZE|GDK_HINT_MIN_SIZE);
  gtk_window_set_default_size(GTK_WINDOW(window), geo.min_width, geo.min_height);

  gtk_paned_set_position(GTK_PANED(window->content_hbox), MAX(400, GTK_WIDGET(window->content_view)->requisition.width));
#endif
}

void
nautilus_window_load_state(NautilusWindow *window, const char *config_path)
{
#if 0
  char *vtype;
  GSList *cur;
  int i, n;
  guint signum;
  char cbuf[1024];

  /* Remove old stuff */
  for(cur = window->nav_views; cur; cur = cur->next)
    nautilus_window_remove_meta_view(window, NAUTILUS_VIEW(cur->data));
  nautilus_window_set_content_view(window, NULL);

  /* Load new stuff */
  gnome_config_push_prefix(config_path);

  vtype = gnome_config_get_string("content_view_type=NONE");
  signum = gtk_signal_lookup("load_state", nautilus_view_get_type());

  if(vtype && strcmp(vtype, "NONE")) /* Create new content view */
    {
      GtkWidget *w;
      GtkType wt;

      wt = gtk_type_from_name(vtype);
      g_assert(wt);
      g_assert(gtk_type_is_a(wt, NAUTILUS_TYPE_VIEW));

      w = gtk_widget_new(wt, "main_window", window, NULL);
      nautilus_window_set_content_view(window, NAUTILUS_VIEW(w));

      g_snprintf(cbuf, sizeof(cbuf), "%s/Content_View/", config_path);
      gnome_config_push_prefix(cbuf);
      gtk_signal_emit(GTK_OBJECT(w), signum, cbuf);
      gnome_config_pop_prefix();

      gtk_widget_show(w);
    }
  g_free(vtype);

  n = gnome_config_get_int("num_nav_views=0");
  for(i = 0; i < n; n++)
    {
      GtkType nvt;
      GtkWidget *nvw;

      g_snprintf(cbuf, sizeof(cbuf), "%s/nav_view_%d_type=0", config_path, i);
      vtype = gnome_config_get_string(cbuf);
      nvt = gtk_type_from_name(vtype);
      g_free(vtype);
      g_assert(nvt);

      g_assert(gtk_type_is_a(nvt, NAUTILUS_TYPE_VIEW));

      nvw = gtk_widget_new(nvt, "main_window", window, NULL);

      g_snprintf(cbuf, sizeof(cbuf), "%s/Nav_View_%d/", config_path, i);

      gnome_config_push_prefix(cbuf);
      gtk_signal_emit(GTK_OBJECT(cur->data), signum, cbuf);
      gnome_config_pop_prefix();

      nautilus_window_add_meta_view(window, NAUTILUS_VIEW(nvw));
    }

  gnome_config_pop_prefix();
#endif
}
