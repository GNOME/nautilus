/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  libnautilus: A library for nautilus view implementations.
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */

/* nautilus-meta-view-frame.c: Implementation for object that represents a
   nautilus meta view implementation. */

#include <config.h>
#include "nautilus-meta-view-frame.h"

#include <libgnome/gnome-i18n.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-property-bag.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>

struct NautilusMetaViewFrameDetails {
	char *label;
};

/* Property indices. */
enum {
	LABEL
};

typedef struct {
	POA_Nautilus_View servant;
	gpointer bonobo_object;
	
	NautilusMetaViewFrame *view;
} impl_POA_Nautilus_MetaView;

extern POA_Nautilus_View__epv libnautilus_Nautilus_View_epv;
static POA_Nautilus_MetaView__epv impl_Nautilus_MetaView_epv;
static PortableServer_ServantBase__epv base_epv;
static POA_Nautilus_MetaView__vepv impl_Nautilus_MetaView_vepv =
{
  &base_epv,
  NULL,
  &libnautilus_Nautilus_View_epv,
  &impl_Nautilus_MetaView_epv
};

static void nautilus_meta_view_frame_initialize       (NautilusMetaViewFrame      *view);
static void nautilus_meta_view_frame_destroy          (NautilusMetaViewFrame      *view);
static void nautilus_meta_view_frame_initialize_class (NautilusMetaViewFrameClass *klass);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusMetaViewFrame, nautilus_meta_view_frame, NAUTILUS_TYPE_VIEW_FRAME)

static void
nautilus_meta_view_frame_initialize (NautilusMetaViewFrame *view)
{
	view->details = g_new0 (NautilusMetaViewFrameDetails, 1);
}

NautilusMetaViewFrame *
nautilus_meta_view_frame_new (GtkWidget *widget)
{
	BonoboControl *control;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
	
	control = bonobo_control_new (widget);
	return nautilus_meta_view_frame_new_from_bonobo_control (control);
}

static void
set_property (BonoboPropertyBag *bag,
	      const BonoboArg *arg,
	      guint arg_id,
	      gpointer user_data)
{
	NautilusMetaViewFrame *view;

	view = NAUTILUS_META_VIEW_FRAME (user_data);
	
	switch (arg_id) {
	case LABEL:
		nautilus_meta_view_frame_set_label (view,
						    BONOBO_ARG_GET_STRING (arg));
		break;
		
	default:
		g_warning ("unknown property");
	}
}

static void
get_property (BonoboPropertyBag *bag,
	      BonoboArg *arg,
	      guint arg_id,
	      gpointer user_data)
{
	NautilusMetaViewFrame *view;

	view = NAUTILUS_META_VIEW_FRAME (user_data);
	
	switch (arg_id) {
	case LABEL:
		BONOBO_ARG_SET_STRING (arg, view->details->label);
		break;
		
	default:
		g_warning ("unknown property");
	}
}

NautilusMetaViewFrame *
nautilus_meta_view_frame_new_from_bonobo_control (BonoboControl *bonobo_control)
{
	NautilusMetaViewFrame *view;
	BonoboPropertyBag *bag;

	g_return_val_if_fail (BONOBO_IS_CONTROL (bonobo_control), NULL);
	g_return_val_if_fail (bonobo_control_get_property_bag (BONOBO_CONTROL (bonobo_control)) == NULL, NULL);

	view = NAUTILUS_META_VIEW_FRAME (gtk_object_new (NAUTILUS_TYPE_META_VIEW_FRAME,
							 "bonobo_control", bonobo_control,
							 NULL));

	bag = bonobo_property_bag_new (get_property, set_property, view);
	bonobo_property_bag_add (bag, "label", LABEL, BONOBO_ARG_STRING, NULL,
				 _("Label"), 0);
	bonobo_control_set_property_bag (bonobo_control, bag);
	
	return view;
}

static void
nautilus_meta_view_frame_destroy (NautilusMetaViewFrame *view)
{
	g_free (view->details->label);
	g_free (view->details);
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, GTK_OBJECT (view));
}

static void
nautilus_meta_view_frame_initialize_class (NautilusMetaViewFrameClass *klass)
{
	NautilusViewFrameClass *view_class;

	view_class = NAUTILUS_VIEW_FRAME_CLASS (klass);
	
	GTK_OBJECT_CLASS (klass)->destroy = (void (*)(GtkObject *)) nautilus_meta_view_frame_destroy;
	
	view_class->servant_init_func = POA_Nautilus_MetaView__init;
	view_class->servant_destroy_func = POA_Nautilus_MetaView__fini;
	view_class->vepv = &impl_Nautilus_MetaView_vepv;
}

void
nautilus_meta_view_frame_set_label (NautilusMetaViewFrame *view,
				    const char *label)
{
	g_return_if_fail (NAUTILUS_IS_META_VIEW_FRAME (view));

	g_free (view->details->label);
	view->details->label = g_strdup (label);
}
