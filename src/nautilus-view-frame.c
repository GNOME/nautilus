/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000 Eazel, Inc.a
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */
/* ntl-view.c: Implementation of the object representing a data view,
   and its associated CORBA object for proxying requests into this
   object. */

#include <config.h>
#include "nautilus-view-frame-private.h"

#include "nautilus-application.h"
#include "nautilus-history-frame.h"
#include "nautilus-window.h"
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <gtk/gtksignal.h>
#include <gtk/gtk.h>
#include <libnautilus-extensions/nautilus-undo-manager.h>
#include <libnautilus/nautilus-view.h>
#include <libnautilus/nautilus-zoomable.h>

enum {
	OPEN_LOCATION,
	OPEN_LOCATION_IN_NEW_WINDOW,
	REPORT_LOCATION_CHANGE,
	REPORT_SELECTION_CHANGE,
	REPORT_STATUS,
	REPORT_LOAD_UNDERWAY,
	REPORT_LOAD_PROGRESS,
	REPORT_LOAD_COMPLETE,
	REPORT_LOAD_FAILED,
	SET_TITLE,
	ZOOM_LEVEL_CHANGED,
	CLIENT_GONE,
	GET_HISTORY_LIST,
	LAST_SIGNAL
};

static void nautilus_view_frame_initialize       (NautilusViewFrame      *view);
static void nautilus_view_frame_destroy          (GtkObject              *view);
static void nautilus_view_frame_initialize_class (NautilusViewFrameClass *klass);

static guint signals[LAST_SIGNAL];

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusViewFrame, nautilus_view_frame, NAUTILUS_TYPE_GENEROUS_BIN)

static void
nautilus_view_frame_initialize_class (NautilusViewFrameClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass*) klass;
	object_class->destroy = nautilus_view_frame_destroy;
	
	signals[OPEN_LOCATION] =
		gtk_signal_new ("open_location",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   open_location),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
	signals[OPEN_LOCATION_IN_NEW_WINDOW] =
		gtk_signal_new ("open_location_in_new_window",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   open_location_in_new_window),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
	signals[REPORT_LOCATION_CHANGE] =
		gtk_signal_new ("report_location_change",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   report_location_change),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
	signals[REPORT_SELECTION_CHANGE] =
		gtk_signal_new ("report_selection_change",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   report_selection_change),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	signals[REPORT_STATUS] =
		gtk_signal_new ("report_status",
				GTK_RUN_LAST,
                    object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   report_status),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
	signals[REPORT_LOAD_UNDERWAY] =
		gtk_signal_new ("report_load_underway",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   report_load_underway),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	signals[REPORT_LOAD_PROGRESS] =
		gtk_signal_new ("report_load_progress",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   report_load_progress),
				nautilus_gtk_marshal_NONE__DOUBLE,
				GTK_TYPE_NONE, 1, GTK_TYPE_DOUBLE);
	signals[REPORT_LOAD_COMPLETE] =
		gtk_signal_new ("report_load_complete",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   report_load_complete),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	signals[REPORT_LOAD_FAILED] =
		gtk_signal_new ("report_load_failed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   report_load_failed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	signals[SET_TITLE] =
		gtk_signal_new ("set_title",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   set_title),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
	
	signals[ZOOM_LEVEL_CHANGED] =
		gtk_signal_new ("zoom_level_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   zoom_level_changed),
				nautilus_gtk_marshal_NONE__DOUBLE,
				GTK_TYPE_NONE, 1, GTK_TYPE_DOUBLE);

	signals[CLIENT_GONE] =
		gtk_signal_new ("client_gone",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   client_gone),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	
	signals[GET_HISTORY_LIST] =
		gtk_signal_new ("get_history_list",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusViewFrameClass, 
						   get_history_list),
				nautilus_gtk_marshal_POINTER__NONE,
				GTK_TYPE_POINTER, 0);
	
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
nautilus_view_frame_initialize (NautilusViewFrame *view)
{
	GTK_WIDGET_SET_FLAGS (view, GTK_NO_WINDOW);
}

static void
nautilus_view_frame_destroy_client (NautilusViewFrame *view)
{
	CORBA_Environment ev;
	CORBA_exception_init(&ev);
	
	if (view->component_class == NULL) {
		return;
	}
	
	g_free (view->iid);
	view->iid = NULL;
	
	bonobo_object_unref (BONOBO_OBJECT (view->client_object));
	view->client_object = NULL;

	gtk_container_remove (GTK_CONTAINER(view), view->client_widget);
	view->client_widget = NULL;

	if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
		Bonobo_Unknown_unref (view->zoomable, &ev);
		CORBA_Object_release (view->zoomable, &ev);
	}
	view->zoomable = CORBA_OBJECT_NIL;

	if (view->component_class->destroy != NULL) {
		view->component_class->destroy (view, &ev);
	}
	
	bonobo_object_unref (view->view_frame);

	view->view_frame = NULL;
	
	view->component_class = NULL;
	view->component_data = NULL;
	
	CORBA_exception_free(&ev);
}

static void
nautilus_view_frame_destroy (GtkObject *object)
{
	NautilusViewFrame *frame;

	frame = NAUTILUS_VIEW_FRAME (object);
	
	if (frame->timer_id != 0) {
		g_source_remove (frame->timer_id);
		frame->timer_id = 0;
	}
	
	nautilus_view_frame_destroy_client (frame);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

extern NautilusViewComponentType nautilus_view_component_type; /* ntl-view-nautilus.c */
extern NautilusViewComponentType bonobo_subdoc_component_type; /* ntl-view-bonobo-subdoc.c */
extern NautilusViewComponentType bonobo_control_component_type; /* ntl-view-bonobo-control.c */

static void
nautilus_view_frame_handle_client_destroy (GtkWidget *widget, NautilusViewFrame *view)
{
	/* FIXME: Is a destroy really sufficient here? Who does the unref? */
	gtk_object_destroy (GTK_OBJECT (view));
}

static void
nautilus_view_frame_handle_client_destroy_2 (GtkObject *object,
					     CORBA_Object cobject,
					     CORBA_Environment *ev,
					     NautilusViewFrame *view)
{
	gtk_signal_emit (object, signals[CLIENT_GONE]);
}

NautilusViewFrame *
nautilus_view_frame_new (BonoboUIHandler *ui_handler,
                         NautilusUndoManager *undo_manager)
{
	NautilusViewFrame *view_frame;
	
	view_frame = NAUTILUS_VIEW_FRAME (gtk_object_new (nautilus_view_frame_get_type (), NULL));
	
	view_frame->ui_handler = ui_handler;
	view_frame->undo_manager = undo_manager;
	
	return view_frame;
}

gboolean /* returns TRUE if successful */
nautilus_view_frame_load_client (NautilusViewFrame *view, const char *iid)
{
	CORBA_Object obj;
	CORBA_Environment ev;
  	int i;
  	
  	NautilusViewComponentType *component_types[] = {
    		&nautilus_view_component_type,
    		&bonobo_subdoc_component_type,
    		&bonobo_control_component_type,
    		NULL
  	};

	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), FALSE);

	nautilus_view_frame_destroy_client (view);

	if (iid == NULL) {
		return FALSE;
        }

	view->client_object = bonobo_object_activate (iid, 0);
	if (view->client_object == NULL) {
		return FALSE;
        }

	CORBA_exception_init (&ev);

	/* Start with a view frame interface. */
	view->view_frame = impl_Nautilus_ViewFrame__create(view, &ev);

	/* Add a zoomable frame interface. */
	view->zoomable_frame = impl_Nautilus_ZoomableFrame__create(view, &ev);
	bonobo_object_add_interface (BONOBO_OBJECT (view->view_frame), 
	                             BONOBO_OBJECT (view->zoomable_frame));

	/* Add a history frame interface. */
	view->history_frame = impl_Nautilus_HistoryFrame__create(view, &ev);
	bonobo_object_add_interface (BONOBO_OBJECT (view->view_frame), 
	                             BONOBO_OBJECT (view->history_frame));

	/* Add an undo context interface. */
	nautilus_undo_manager_add_interface
        	(view->undo_manager, BONOBO_OBJECT (view->view_frame));
	
	/* Get at our client's zoomable interface. */
	view->zoomable = bonobo_object_query_interface
		(BONOBO_OBJECT (view->client_object), 
		 "IDL:Nautilus/Zoomable:1.0");
	
	/* Now figure out which type of embedded object we have so we
	 * can host it appropriately:
	 */
	for (i = 0; component_types[i] != NULL && view->component_class == NULL; i++) {
		obj = Bonobo_Unknown_query_interface
			(bonobo_object_corba_objref (BONOBO_OBJECT (view->client_object)),
			 component_types[i]->primary_repoid, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
        		obj = CORBA_OBJECT_NIL;
		}
		if (CORBA_Object_is_nil (obj, &ev)) {
			continue;
		}

      		if (component_types[i]->try_load (view, obj, &ev)) {
        		view->component_class = component_types[i];
		}

      		Bonobo_Unknown_unref (obj, &ev);
      		CORBA_Object_release (obj, &ev);
    	}

	/* Handle case where we don't know how to host this component. */
  	if (view->component_class == NULL) {
      		nautilus_view_frame_destroy_client (view);
      		return FALSE;
    	}
	
	view->iid = g_strdup (iid);

	gtk_signal_connect_while_alive
		(GTK_OBJECT (view->client_object), "destroy",
		 GTK_SIGNAL_FUNC (nautilus_view_frame_handle_client_destroy), view,
		 GTK_OBJECT (view));
	gtk_signal_connect_while_alive
		(GTK_OBJECT (view->client_object), "object_gone",
		 GTK_SIGNAL_FUNC (nautilus_view_frame_handle_client_destroy_2), view,
		 GTK_OBJECT (view));
	gtk_signal_connect_while_alive
		(GTK_OBJECT (view->client_object), "system_exception",
		 GTK_SIGNAL_FUNC (nautilus_view_frame_handle_client_destroy_2), view,
		 GTK_OBJECT (view));
	gtk_container_add (GTK_CONTAINER (view), view->client_widget);
	gtk_widget_show (view->client_widget);
	CORBA_exception_free (&ev);

	return TRUE;
}

void
nautilus_view_frame_load_location (NautilusViewFrame *view,
                                   const char *location)
{
	CORBA_Environment ev;
	
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	g_return_if_fail (view->component_class != NULL);
	
	if (view->component_class->load_location == NULL) {
		return;
	}
	
	CORBA_exception_init(&ev);
	/* ORBit does a bad job with Nautilus_URI, so it's not const char *. */
	view->component_class->load_location(view, (Nautilus_URI) location, &ev);
	CORBA_exception_free(&ev);
}

void
nautilus_view_frame_stop_loading (NautilusViewFrame *view)
{
	CORBA_Environment ev;
	
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	g_return_if_fail (view->component_class != NULL);
	
	if (view->component_class->stop_loading == NULL) {
		return;
	}

	CORBA_exception_init(&ev);
	view->component_class->stop_loading(view, &ev);
	CORBA_exception_free(&ev);
}

void
nautilus_view_frame_selection_changed (NautilusViewFrame *view,
                                       GList *selection)
{
	Nautilus_URIList *uri_list;
	
	CORBA_Environment ev;
	
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	g_return_if_fail (view->component_class != NULL);
	
	if (view->component_class->selection_changed == NULL) {
		return;
	}
	
	uri_list = nautilus_uri_list_from_g_list (selection);
	
	CORBA_exception_init(&ev);
	view->component_class->selection_changed(view, uri_list, &ev);
	CORBA_exception_free(&ev);
	
	CORBA_free (uri_list);
}

gboolean
nautilus_view_frame_is_zoomable (NautilusViewFrame *view)
{
	CORBA_Environment ev;
	gboolean is_zoomable;
	
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), FALSE);
	
	CORBA_exception_init (&ev);
	is_zoomable = !CORBA_Object_is_nil (view->zoomable, &ev);
	CORBA_exception_free (&ev);
	
	return is_zoomable;
}

gdouble
nautilus_view_frame_get_zoom_level (NautilusViewFrame *view)
{
	CORBA_Environment ev;
	double level;
	
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), 0);
	
	CORBA_exception_init (&ev);
	
	if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
		level = Nautilus_Zoomable__get_zoom_level (view->zoomable, &ev);
	} else {
		level = 1.0;
	}

	CORBA_exception_free (&ev);
	
	return level;
}

void
nautilus_view_frame_set_zoom_level (NautilusViewFrame *view,
				    double zoom_level)
{
	CORBA_Environment ev;
	
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	
	CORBA_exception_init (&ev);
	
	if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
		Nautilus_Zoomable__set_zoom_level (view->zoomable, zoom_level, &ev);
	} else {
		/* do nothing */
	}
	
	CORBA_exception_free (&ev);
}

gdouble
nautilus_view_frame_get_min_zoom_level (NautilusViewFrame *view)
{
	CORBA_Environment ev;
	double level;
	
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), 0);
	
	CORBA_exception_init (&ev);
	
	if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
		level = Nautilus_Zoomable__get_min_zoom_level (view->zoomable, &ev);
	} else {
		level = 1.0;
	}
	
	CORBA_exception_free (&ev);
	
	return level;
}

double
nautilus_view_frame_get_max_zoom_level (NautilusViewFrame *view)
{
	CORBA_Environment ev;
	double level;
	
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), 0);
	
	CORBA_exception_init (&ev);
	
	if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
		level = Nautilus_Zoomable__get_max_zoom_level (view->zoomable, &ev);
	} else {
		level = 1.0;
	}
	
	CORBA_exception_free (&ev);
	
	return level;
}

GList *
nautilus_view_frame_get_preferred_zoom_levels (NautilusViewFrame *view)
{
	CORBA_Environment ev;
	GList *levels;
	Nautilus_ZoomLevelList *zoom_levels;
	
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), 0);
	
	CORBA_exception_init (&ev);
	
	if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
		zoom_levels = Nautilus_Zoomable__get_preferred_zoom_levels (view->zoomable, &ev);
		levels = nautilus_g_list_from_ZoomLevelList (zoom_levels);
		CORBA_free (zoom_levels);
		
	} else {
		levels = NULL;
	}
	
	CORBA_exception_free (&ev);
	
	return levels;
}

void
nautilus_view_frame_zoom_in (NautilusViewFrame *view)
{
	CORBA_Environment ev;
	
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	
	CORBA_exception_init (&ev);
	
	if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
		Nautilus_Zoomable_zoom_in (view->zoomable, &ev);
	} else {
		/* do nothing */
	}
	
	CORBA_exception_free (&ev);
}

void
nautilus_view_frame_zoom_out (NautilusViewFrame *view)
{
	CORBA_Environment ev;
	
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	
	CORBA_exception_init (&ev);
	
	if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
		Nautilus_Zoomable_zoom_out (view->zoomable, &ev);
	} else {
		/* do nothing */
	}
	
	CORBA_exception_free (&ev);
}

void
nautilus_view_frame_zoom_to_fit (NautilusViewFrame *view)
{
	CORBA_Environment ev;
	
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	
	CORBA_exception_init (&ev);
	
	if (!CORBA_Object_is_nil (view->zoomable, &ev)) {
		Nautilus_Zoomable_zoom_to_fit (view->zoomable, &ev);
	} else {
		/* do nothing */
	}
	
	CORBA_exception_free (&ev);
}

const char *
nautilus_view_frame_get_iid(NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), NULL);
	return view->iid;
}

CORBA_Object
nautilus_view_frame_get_client_objref (NautilusViewFrame *view)
{
	g_return_val_if_fail (view == NULL || NAUTILUS_IS_VIEW_FRAME (view), CORBA_OBJECT_NIL);
	return view == NULL ? CORBA_OBJECT_NIL : bonobo_object_corba_objref (BONOBO_OBJECT (view->client_object));
}

CORBA_Object
nautilus_view_frame_get_objref (NautilusViewFrame *view)
{
	g_return_val_if_fail (view == NULL || NAUTILUS_IS_VIEW_FRAME (view), CORBA_OBJECT_NIL);
	return view == NULL ? CORBA_OBJECT_NIL : bonobo_object_corba_objref (view->view_frame);
}

void
nautilus_view_frame_open_location (NautilusViewFrame *view,
                                   const char *location)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	gtk_signal_emit (GTK_OBJECT (view), signals[OPEN_LOCATION], location);
}

void
nautilus_view_frame_open_location_in_new_window (NautilusViewFrame *view,
                                                 const char *location)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	gtk_signal_emit (GTK_OBJECT (view), signals[OPEN_LOCATION_IN_NEW_WINDOW], location);
}

void
nautilus_view_frame_report_location_change (NautilusViewFrame *view,
                                            const char *location)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	gtk_signal_emit (GTK_OBJECT (view), signals[REPORT_LOCATION_CHANGE], location);
}

void
nautilus_view_frame_report_selection_change (NautilusViewFrame *view,
                                             GList *selection)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	gtk_signal_emit (GTK_OBJECT (view), signals[REPORT_SELECTION_CHANGE], selection);
}

void
nautilus_view_frame_report_status (NautilusViewFrame *view,
                                   const char *status)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	gtk_signal_emit (GTK_OBJECT (view), signals[REPORT_STATUS], status);
}

void
nautilus_view_frame_report_load_underway (NautilusViewFrame *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	gtk_signal_emit (GTK_OBJECT (view), signals[REPORT_LOAD_UNDERWAY]);
}

void
nautilus_view_frame_report_load_progress (NautilusViewFrame *view,
                                          double fraction_done)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	gtk_signal_emit (GTK_OBJECT (view), signals[REPORT_LOAD_PROGRESS], fraction_done);
}

void
nautilus_view_frame_report_load_complete (NautilusViewFrame *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	gtk_signal_emit (GTK_OBJECT (view), signals[REPORT_LOAD_COMPLETE]);
}

void
nautilus_view_frame_report_load_failed (NautilusViewFrame *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	gtk_signal_emit (GTK_OBJECT (view), signals[REPORT_LOAD_FAILED]);
}

void
nautilus_view_frame_set_title (NautilusViewFrame *view,
                               const char *title)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	gtk_signal_emit (GTK_OBJECT (view), signals[SET_TITLE], title);
}

void
nautilus_view_frame_zoom_level_changed (NautilusViewFrame *view,
                                        double level)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	gtk_signal_emit (GTK_OBJECT (view), signals[ZOOM_LEVEL_CHANGED], level);
}

static gboolean
check_object (gpointer data)
{
	NautilusViewFrame *view;
	CORBA_Environment ev;
	gboolean ok;

	view = NAUTILUS_VIEW_FRAME (data);
	g_assert (!view->checking);

	CORBA_exception_init (&ev);
	view->checking++;
	ok = TRUE;
	if (CORBA_Object_non_existent (bonobo_object_corba_objref (BONOBO_OBJECT (view->client_object)), &ev)) {
		/* FIXME bugzilla.eazel.com 1840: Is a destroy really sufficient here? Who does the unref? 
		 * See bug 1840 for one bad case this destroy is involved in.
		 */
		gtk_object_destroy (GTK_OBJECT (view));

		view->timer_id = 0;
		ok = FALSE;
	}
	view->checking--;
	CORBA_exception_free (&ev);
	
	return ok;
}

void
nautilus_view_frame_set_active_errors (NautilusViewFrame *view, gboolean enabled)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	if (enabled) {
		if (view->timer_id == 0) {
			/* FIXME: Is a hard-coded 2-second timeout acceptable? */
			view->timer_id = g_timeout_add (2000, check_object, view);
		}
	} else {
		if (view->timer_id != 0) {
			g_source_remove (view->timer_id);
			view->timer_id = 0;
		}
	}
}

char *
nautilus_view_frame_get_label (NautilusViewFrame *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), NULL);
	return g_strdup (view->label);
}

void
nautilus_view_frame_set_label (NautilusViewFrame *view,
                               const char *label)
{
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (view));
	g_free (view->label);
	view->label = g_strdup (label);
}

/* Calls activate on the underlying control frame. */
void
nautilus_view_frame_activate (NautilusViewFrame *view)
{
	BonoboControlFrame *control_frame;
	
	control_frame = BONOBO_CONTROL_FRAME (bonobo_object_query_local_interface 
					      (view->view_frame, "IDL:Bonobo/ControlFrame:1.0"));
	bonobo_control_frame_control_activate (control_frame);
	bonobo_object_unref (BONOBO_OBJECT (control_frame));
}


Nautilus_History *
nautilus_view_frame_get_history_list (NautilusViewFrame *view)
{
	Nautilus_History *history_list;
	
	g_return_val_if_fail (NAUTILUS_IS_VIEW_FRAME (view), NULL);
	
	/* Sent out signal to get the history list. */
	history_list = NULL;
	gtk_signal_emit (GTK_OBJECT (view), 
  			 signals[GET_HISTORY_LIST],
			 &history_list);
  	return history_list;
}
