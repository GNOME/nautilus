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

#include <config.h>
#include "nautilus.h"
#include <libnautilus/libnautilus.h>
#include <bonobo.h>
#include "file-manager/fm-icon-view.h"
#include "file-manager/fm-list-view.h"
#include <libnautilus-extensions/nautilus-global-preferences.h>

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
				   CORBA_char *obj_iid,
				   CORBA_Environment * ev);
static CORBA_Object
impl_Nautilus_Application_create_object(impl_POA_Nautilus_Application *servant,
                                        CORBA_char *obj_iid,
					GNOME_stringlist * params,
					CORBA_Environment * ev);

static POA_Nautilus_Application__epv impl_Nautilus_Application_epv = {
   NULL,			/* _private */
   (gpointer) &impl_Nautilus_Application__get_view_windows,
   (gpointer) &impl_Nautilus_Application_new_view_window

};
static POA_Bonobo_GenericFactory__epv impl_Nautilus_Application_Bonobo_GenericFactory_epv = {
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
   &impl_Nautilus_Application_Bonobo_GenericFactory_epv,
   NULL,
   &impl_Nautilus_Application_epv,
};

static Nautilus_ViewWindowList *
impl_Nautilus_Application__get_view_windows(impl_POA_Nautilus_Application *servant,
                                            CORBA_Environment * ev)
{
   Nautilus_ViewWindowList *retval;

   retval = Nautilus_ViewWindowList__alloc();
   retval->_length = g_slist_length(servant->app->windows);

   if(retval->_length)
     {
       int i;
       GSList *ltmp;

       retval->_buffer = CORBA_sequence_Nautilus_ViewWindow_allocbuf(retval->_length);
       for(i = 0, ltmp = servant->app->windows; ltmp; ltmp = ltmp->next, i++)
         {
           CORBA_Object obj;
           obj = bonobo_object_corba_objref(NAUTILUS_WINDOW(ltmp->data)->ntl_viewwindow);
           retval->_buffer[i] = CORBA_Object_duplicate(obj, ev);
         }
     }
   else
     retval->_buffer = NULL;

   CORBA_sequence_set_release(retval, CORBA_TRUE);

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
				   CORBA_char * obj_iid,
				   CORBA_Environment * ev)
{
   return (!strcmp(obj_iid, "OAFIID:ntl_file_manager_icon_view:42681b21-d5ca-4837-87d2-394d88ecc058")
           || !strcmp(obj_iid, "OAFIID:ntl_file_manager_list_view:521e489d-0662-4ad7-ac3a-832deabe111c")
           || !strcmp(obj_iid, "OAFIID:ntl_window:88e8b2e4-b627-4221-b566-5ba32185c88d"));
}

static CORBA_Object
impl_Nautilus_Application_create_object(impl_POA_Nautilus_Application *servant,
                                        CORBA_char *obj_iid,
					GNOME_stringlist * params,
					CORBA_Environment * ev)
{
  CORBA_Object retval = CORBA_OBJECT_NIL;
  FMDirectoryView *dir_view;
  NautilusContentViewFrame *view_frame;

  if(!impl_Nautilus_Application_supports(servant, obj_iid, ev))
    return CORBA_OBJECT_NIL;
        
  if (strcmp (obj_iid, "OAFIID:ntl_file_manager_icon_view:42681b21-d5ca-4837-87d2-394d88ecc058") == 0)
    dir_view = FM_DIRECTORY_VIEW (gtk_object_new (fm_icon_view_get_type (), NULL));
  else if (strcmp (obj_iid, "OAFIID:ntl_file_manager_list_view:521e489d-0662-4ad7-ac3a-832deabe111c") == 0)
    dir_view = FM_DIRECTORY_VIEW (gtk_object_new (fm_list_view_get_type (), NULL));
  else if (strcmp (obj_iid,"OAFIID:ntl_window:88e8b2e4-b627-4221-b566-5ba32185c88d") == 0)
    retval = impl_Nautilus_Application_new_view_window (servant, ev);
  else
    dir_view = NULL;
        
  g_return_val_if_fail(dir_view, CORBA_OBJECT_NIL);
        
  if(dir_view)
    {
      view_frame = fm_directory_view_get_view_frame (dir_view);

      retval = CORBA_Object_duplicate(bonobo_object_corba_objref(BONOBO_OBJECT(view_frame)), ev);
    }

  return retval;
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

	/*
	 * Startup preferences.  This call is needed so that preferences have
	 * decent defaults.  Not calling this function would cause some
	 * preferences to have "not nice" defaults.  Things would still work
	 * because it is legal to use nautilus preferences implicitly. 
	 */
	nautilus_global_preferences_startup ();
}

static void
nautilus_app_init (NautilusApp *app)
{
  CORBA_Environment ev;
  CORBA_Object objref;

  CORBA_exception_init(&ev);
  objref = impl_Nautilus_Application__create(bonobo_poa(), app, &ev);

  oaf_active_server_register("OAFIID:ntl_file_manager_factory:bd1e1862-92d7-4391-963e-37583f0daef3", objref);

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
	/*
	 * Shutdown preferences.  This is needed so that the global preferences
	 * obejct and all its allocations are freed.  Not calling this function
	 * would have NOT cause the user to lose preferences.  The only effect
	 * would be to leak those objects - which would be collected at exit()
	 * time anyway, but it adds noice to memory profile tool runs. 
	 */
	nautilus_global_preferences_shutdown ();

  nautilus_bookmarks_exiting();
  GTK_OBJECT_CLASS(app_parent_class)->destroy(object);
}

void
nautilus_app_startup(NautilusApp *app, const char *initial_url)
{
  NautilusWindow *mainwin;

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
  GtkWidget *win = GTK_WIDGET (gtk_object_new (nautilus_window_get_type(), "app_id", "nautilus", 
                               "app", BONOBO_OBJECT(app), NULL));


  gtk_signal_connect(GTK_OBJECT(win), "destroy", nautilus_app_destroy_window, app);

  /* Do not yet show the window. It will be shown later on if it can
   * successfully display its initial uri. Otherwise it will be destroyed
   * without ever having seen the light of day.
   */

  app->windows = g_slist_prepend(app->windows, win);

  return NAUTILUS_WINDOW(win);
}
