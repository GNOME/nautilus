/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>,
 *
 */

#include "nautilus.h"
#include <libnautilus/libnautilus.h>
#include <bonobo.h>
#include "file-manager/fm-directory-view-icons.h"
#include "file-manager/fm-directory-view-list.h"

typedef struct {
  POA_Nautilus_Application servant;
  NautilusApp *app;

  Nautilus_ViewWindowList attr_view_windows;
} impl_POA_Nautilus_Application;

static Nautilus_ViewWindowList *
impl_Nautilus_Application__get_view_windows(impl_POA_Nautilus_Application* servant,
                                            CORBA_Environment * ev);
static Nautilus_ViewWindow
impl_Nautilus_Application_new_view_window(impl_POA_Nautilus_Application *servant,
                                          CORBA_Environment * ev);
static CORBA_boolean
impl_Nautilus_Application_supports(impl_POA_Nautilus_Application * servant,
				   CORBA_char *obj_goad_id,
				   CORBA_Environment * ev);
static CORBA_Object
impl_Nautilus_Application_create_object(impl_POA_Nautilus_Application *servant,
                                        CORBA_char * goad_id,
					GNOME_stringlist * params,
					CORBA_Environment * ev);

static POA_Nautilus_Application__epv impl_Nautilus_Application_epv = {
   NULL,			/* _private */
   (gpointer) &impl_Nautilus_Application__get_view_windows,
   (gpointer) &impl_Nautilus_Application_new_view_window

};
static POA_GNOME_GenericFactory__epv impl_Nautilus_Application_GNOME_GenericFactory_epv = {
   NULL,			/* _private */
   (gpointer) &impl_Nautilus_Application_supports,
   (gpointer) &impl_Nautilus_Application_create_object
};
static PortableServer_ServantBase__epv impl_Nautilus_Application_base_epv = {
   NULL,			/* _private data */
   NULL,			/* finalize routine */
   NULL,			/* default_POA routine */
};
static POA_Nautilus_Application__vepv impl_Nautilus_Application_vepv = {
   &impl_Nautilus_Application_base_epv,
   &impl_Nautilus_Application_GNOME_GenericFactory_epv,
   NULL,
   &impl_Nautilus_Application_epv,
};

static Nautilus_ViewWindowList *
impl_Nautilus_Application__get_view_windows(impl_POA_Nautilus_Application *
					    servant, CORBA_Environment * ev)
{
   Nautilus_ViewWindowList *retval;

   return retval;
}

static Nautilus_ViewWindow
impl_Nautilus_Application_new_view_window(impl_POA_Nautilus_Application *servant, CORBA_Environment * ev)
{
  NautilusWindow *win;

  win = nautilus_app_create_window(servant->app);

  return CORBA_Object_duplicate(bonobo_object_corba_objref(BONOBO_OBJECT(win)), ev);
}

static CORBA_boolean
impl_Nautilus_Application_supports(impl_POA_Nautilus_Application * servant,
				   CORBA_char * obj_goad_id,
				   CORBA_Environment * ev)
{
   return (!strcmp(obj_goad_id, "ntl_file_manager_icon_view")
           || !strcmp(obj_goad_id, "ntl_file_manager_list_view"));
}

static CORBA_Object
impl_Nautilus_Application_create_object(impl_POA_Nautilus_Application *servant,
                                        CORBA_char * goad_id,
					GNOME_stringlist * params,
					CORBA_Environment * ev)
{
  FMDirectoryView *dir_view;
  NautilusContentViewFrame *view_frame;

  if(!impl_Nautilus_Application_supports(servant, goad_id, ev))
    return CORBA_OBJECT_NIL;
        
  if (strcmp (goad_id, "ntl_file_manager_icon_view") == 0)
    dir_view = FM_DIRECTORY_VIEW (gtk_object_new (fm_directory_view_icons_get_type (), NULL));
  else if (strcmp (goad_id, "ntl_file_manager_list_view") == 0)
    dir_view = FM_DIRECTORY_VIEW (gtk_object_new (fm_directory_view_list_get_type (), NULL));
  else
    dir_view = NULL;
        
  g_return_val_if_fail(dir_view, CORBA_OBJECT_NIL);
        
  view_frame = fm_directory_view_get_view_frame (dir_view);
        
  return CORBA_Object_duplicate(bonobo_object_corba_objref(BONOBO_OBJECT(view_frame)), ev);
}


static CORBA_Object
impl_Nautilus_Application__create(PortableServer_POA poa,
                                  NautilusApp *app,
				  CORBA_Environment * ev)
{
   impl_POA_Nautilus_Application *newservant;

   newservant = g_new0(impl_POA_Nautilus_Application, 1);
   newservant->servant.vepv = &impl_Nautilus_Application_vepv;
   newservant->servant.vepv->Bonobo_Unknown_epv = NAUTILUS_APP_CLASS(GTK_OBJECT(app)->klass)->unknown_epv;
   POA_Nautilus_Application__init((PortableServer_Servant) newservant, ev);
   return bonobo_object_activate_servant(BONOBO_OBJECT(app), newservant);
}

static void nautilus_app_init		(NautilusApp		 *app);
static void nautilus_app_class_init	(NautilusAppClass	 *klass);
static void nautilus_app_destroy        (GtkObject *object);

static GtkObjectClass *app_parent_class = NULL;

GtkType
nautilus_app_get_type (void)
{
  static GtkType App_type = 0;

  if (!App_type)
    {
      static const GtkTypeInfo App_info =
      {
        "NautilusApp",
        sizeof (NautilusApp),
        sizeof (NautilusAppClass),
        (GtkClassInitFunc) nautilus_app_class_init,
        (GtkObjectInitFunc) nautilus_app_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      App_type = gtk_type_unique (bonobo_object_get_type (), &App_info);
    }

  return App_type;
}

static void
nautilus_app_class_init (NautilusAppClass *klass)
{
  GtkObjectClass *object_class;

  klass->unknown_epv = bonobo_object_get_epv();

  object_class = (GtkObjectClass*) klass;
  object_class->destroy = nautilus_app_destroy;

  app_parent_class = gtk_type_class (gtk_object_get_type ());
}

static void
nautilus_app_init (NautilusApp *app)
{
  CORBA_Environment ev;
  CORBA_Object objref;

  CORBA_exception_init(&ev);
  objref = impl_Nautilus_Application__create(bonobo_poa(), app, &ev);
  goad_server_register(NULL, objref, "ntl_file_manager_factory", NULL, &ev);
  bonobo_object_construct(BONOBO_OBJECT(app), objref);
  CORBA_exception_free(&ev);
}

GtkObject *
nautilus_app_new      (void)
{
  return gtk_object_new(nautilus_app_get_type(), NULL);
}

static void
nautilus_app_destroy(GtkObject *object)
{
  /* Do those things that gotta be done just once before quitting */
  nautilus_prefs_save();
  nautilus_bookmarks_exiting();
  GTK_OBJECT_CLASS(app_parent_class)->destroy(object);
}

void
nautilus_app_startup(NautilusApp *app, const char *initial_url)
{
  NautilusWindow *mainwin;

  nautilus_navinfo_init();
  nautilus_prefs_load();

  /* Set default configuration */
  mainwin = nautilus_app_create_window(app);
  bonobo_activate();
  nautilus_window_set_initial_state(mainwin, initial_url);
}

static void
nautilus_app_destroy_window(GtkObject *obj, NautilusApp *app)
{
  app->windows = g_slist_remove(app->windows, obj);

  if(!app->windows)
    gtk_main_quit();
}

NautilusWindow *
nautilus_app_create_window(NautilusApp *app)
{
  GtkWidget *win = gtk_widget_new(nautilus_window_get_type(), "app_id", "nautilus", NULL);

  gtk_signal_connect(GTK_OBJECT(win), "destroy", nautilus_app_destroy_window, app);

  /* Do not yet show the window. It will be shown later on if it can
   * successfully display its initial uri. Otherwise it will be destroyed
   * without ever having seen the light of day.
   */

  app->windows = g_slist_prepend(app->windows, win);

  return NAUTILUS_WINDOW(win);
}
