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
#include <libnautilus-extensions/nautilus-gtk-macros.h>

typedef struct {
	POA_Nautilus_View servant;
	gpointer bonobo_object;
	
	NautilusMetaView *view;
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

static void nautilus_meta_view_initialize       (NautilusMetaView      *view);
static void nautilus_meta_view_destroy          (NautilusMetaView      *view);
static void nautilus_meta_view_initialize_class (NautilusMetaViewClass *klass);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusMetaView, nautilus_meta_view, NAUTILUS_TYPE_VIEW)

static void
nautilus_meta_view_initialize (NautilusMetaView *view)
{
}

NautilusMetaView *
nautilus_meta_view_new (GtkWidget *widget)
{
	BonoboControl *control;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
	
	control = bonobo_control_new (widget);
	return nautilus_meta_view_new_from_bonobo_control (control);
}


NautilusMetaView *
nautilus_meta_view_new_from_bonobo_control (BonoboControl *bonobo_control)
{
	NautilusMetaView *view;

	g_return_val_if_fail (BONOBO_IS_CONTROL (bonobo_control), NULL);

	view = NAUTILUS_META_VIEW (gtk_object_new (NAUTILUS_TYPE_META_VIEW,
							 "bonobo_control", bonobo_control,
							 NULL));

	
	return view;
}

static void
nautilus_meta_view_destroy (NautilusMetaView *view)
{
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, GTK_OBJECT (view));
}

static void
nautilus_meta_view_initialize_class (NautilusMetaViewClass *klass)
{
	NautilusViewClass *view_class;

	view_class = NAUTILUS_VIEW_CLASS (klass);
	
	GTK_OBJECT_CLASS (klass)->destroy = (void (*)(GtkObject *)) nautilus_meta_view_destroy;
	
	view_class->servant_init_func = POA_Nautilus_MetaView__init;
	view_class->servant_destroy_func = POA_Nautilus_MetaView__fini;
	view_class->vepv = &impl_Nautilus_MetaView_vepv;
}

