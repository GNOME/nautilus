/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-view.c: Interface for nautilus views
 
   Copyright (C) 2004 Red Hat Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include "nautilus-view.h"

enum {
	TITLE_CHANGED,
	ZOOM_PARAMETERS_CHANGED,
	ZOOM_LEVEL_CHANGED,
	LAST_SIGNAL
};

static guint nautilus_view_signals[LAST_SIGNAL] = { 0 };

static void
nautilus_view_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (! initialized) {
		nautilus_view_signals[TITLE_CHANGED] =
			g_signal_new ("title_changed",
				      NAUTILUS_TYPE_VIEW,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (NautilusViewIface, title_changed),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);
		
		nautilus_view_signals[ZOOM_PARAMETERS_CHANGED] =
			g_signal_new ("zoom_parameters_changed",
				      NAUTILUS_TYPE_VIEW,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (NautilusViewIface, zoom_parameters_changed),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);
		
		nautilus_view_signals[ZOOM_LEVEL_CHANGED] =
			g_signal_new ("zoom_level_changed",
				      NAUTILUS_TYPE_VIEW,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (NautilusViewIface, zoom_level_changed),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);
		
		initialized = TRUE;
	}
}

GType                   
nautilus_view_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (NautilusViewIface),
			nautilus_view_base_init,
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			0,
			NULL
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE, 
					       "NautilusView",
					       &info, 0);
		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}
	
	return type;
}

const char *
nautilus_view_get_view_id (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);
	
	return (* NAUTILUS_VIEW_GET_IFACE (view)->get_view_id) (view);
}

GtkWidget *
nautilus_view_get_widget (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);
	
	return (* NAUTILUS_VIEW_GET_IFACE (view)->get_widget) (view);
}

void
nautilus_view_load_location (NautilusView *view,
			     const char   *location_uri)
{
	g_return_if_fail (NAUTILUS_IS_VIEW (view));
	g_return_if_fail (location_uri != NULL);
	
	(* NAUTILUS_VIEW_GET_IFACE (view)->load_location) (view,
							   location_uri);
}

void
nautilus_view_stop_loading (NautilusView *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW (view));
	
	(* NAUTILUS_VIEW_GET_IFACE (view)->stop_loading) (view);
}

int
nautilus_view_get_selection_count (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), 0);
	
	return (* NAUTILUS_VIEW_GET_IFACE (view)->get_selection_count) (view);
}

GList *
nautilus_view_get_selection (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);
	
	return (* NAUTILUS_VIEW_GET_IFACE (view)->get_selection) (view);
}

void
nautilus_view_set_selection (NautilusView *view,
			     GList        *list)
{
	g_return_if_fail (NAUTILUS_IS_VIEW (view));
	
	(* NAUTILUS_VIEW_GET_IFACE (view)->set_selection) (view,
							   list);
}


char *
nautilus_view_get_first_visible_file (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);
	
	return (* NAUTILUS_VIEW_GET_IFACE (view)->get_first_visible_file) (view);
}

void
nautilus_view_scroll_to_file (NautilusView *view,
			      const char   *uri)
{
	g_return_if_fail (NAUTILUS_IS_VIEW (view));
	
	(* NAUTILUS_VIEW_GET_IFACE (view)->scroll_to_file) (view, uri);
}

char *
nautilus_view_get_title (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);
	
	if (NAUTILUS_VIEW_GET_IFACE (view)->get_title != NULL) {
		return (* NAUTILUS_VIEW_GET_IFACE (view)->get_title) (view);
	} else {
		return NULL;
	}
}

gboolean
nautilus_view_get_is_zoomable (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), FALSE);
	
	return (* NAUTILUS_VIEW_GET_IFACE (view)->get_is_zoomable) (view);
}

float
nautilus_view_get_zoom_level (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), 1.0);
	
	return (* NAUTILUS_VIEW_GET_IFACE (view)->get_zoom_level) (view);
}

void
nautilus_view_set_zoom_level (NautilusView *view,
			      float         zoom_level)
{
	g_return_if_fail (NAUTILUS_IS_VIEW (view));
	
	(* NAUTILUS_VIEW_GET_IFACE (view)->set_zoom_level) (view,
							    zoom_level);
}

float
nautilus_view_get_min_zoom_level (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), 1.0);
	
	return (* NAUTILUS_VIEW_GET_IFACE (view)->get_min_zoom_level) (view);
}

float
nautilus_view_get_max_zoom_level (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), 1.0);
	
	return (* NAUTILUS_VIEW_GET_IFACE (view)->get_max_zoom_level) (view);
}

gboolean
nautilus_view_get_has_min_zoom_level (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), FALSE);
	
	return (* NAUTILUS_VIEW_GET_IFACE (view)->get_has_min_zoom_level) (view);
}

gboolean
nautilus_view_get_has_max_zoom_level (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), FALSE);
	
	return (* NAUTILUS_VIEW_GET_IFACE (view)->get_has_max_zoom_level) (view);
}

gboolean
nautilus_view_get_is_continuous (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), FALSE);
	
	return (* NAUTILUS_VIEW_GET_IFACE (view)->get_is_continuous) (view);
}

gboolean
nautilus_view_get_can_zoom_in (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), FALSE);
	
	return (* NAUTILUS_VIEW_GET_IFACE (view)->get_can_zoom_in) (view);
}

gboolean
nautilus_view_get_can_zoom_out (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), FALSE);
	
	return (* NAUTILUS_VIEW_GET_IFACE (view)->get_can_zoom_out) (view);
}

GList *
nautilus_view_get_preferred_zoom_levels (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);
	
	return (* NAUTILUS_VIEW_GET_IFACE (view)->get_preferred_zoom_levels) (view);
}

void
nautilus_view_zoom_in (NautilusView *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW (view));
	
	(* NAUTILUS_VIEW_GET_IFACE (view)->zoom_in) (view);
}

void
nautilus_view_zoom_out (NautilusView *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW (view));
	
	(* NAUTILUS_VIEW_GET_IFACE (view)->zoom_out) (view);
}

void
nautilus_view_zoom_to_fit (NautilusView *view)
{
	g_return_if_fail (NAUTILUS_IS_VIEW (view));
	
	(* NAUTILUS_VIEW_GET_IFACE (view)->zoom_to_fit) (view);
}

static void
nautilus_ui_component_free (BonoboUIComponent *ui_component)
{
	bonobo_ui_component_unset_container (ui_component, NULL);
	bonobo_object_unref (BONOBO_OBJECT (ui_component));
}

static BonoboUIComponent *
nautilus_view_get_ui_component (NautilusView *view)
{
	BonoboUIComponent *ui_component;
	
	ui_component = g_object_get_data (G_OBJECT (view),
					  "nautilus-view-ui-component");

	if (ui_component == NULL) {
		ui_component = bonobo_ui_component_new_default ();
		g_object_set_data_full (G_OBJECT (view),
					"nautilus-view-ui-component",
					ui_component,
					(GDestroyNotify) nautilus_ui_component_free);
	}
	
	return ui_component;
}
 
 
BonoboUIComponent *
nautilus_view_set_up_ui (NautilusView *view,
			 Bonobo_UIContainer ui_container,
			 const char *datadir,
			 const char *ui_file_name,
			 const char *application_name)
{
	BonoboUIComponent *ui_component;

	/* Get the UI component that's pre-made by the control. */
	ui_component = nautilus_view_get_ui_component (view);

	/* Connect the UI component to the control frame's UI container. */
	bonobo_ui_component_set_container (ui_component, ui_container, NULL);

	/* Set up the UI from an XML file. */
	bonobo_ui_util_set_ui (ui_component, datadir, ui_file_name, application_name, NULL);

	return ui_component;
}

