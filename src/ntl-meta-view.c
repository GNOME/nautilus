/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */

/* ntl-meta-view.c: Implementation of the object representing a meta/navigation view. */

#include <config.h>
#include "nautilus.h"
#include "ntl-view-private.h"
#include <gtk/gtksignal.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>

static PortableServer_ServantBase__epv base_epv;
static POA_Nautilus_MetaViewFrame__epv impl_Nautilus_MetaViewFrame_epv;

extern POA_Nautilus_ViewFrame__epv impl_Nautilus_ViewFrame_epv;
static POA_Nautilus_MetaViewFrame__vepv impl_Nautilus_MetaViewFrame_vepv =
{
	&base_epv,
	NULL,
	&impl_Nautilus_ViewFrame_epv,
	&impl_Nautilus_MetaViewFrame_epv
};

static void nautilus_meta_view_frame_initialize_class (NautilusMetaViewFrameClass *klass);
static void nautilus_meta_view_frame_initialize (NautilusMetaViewFrame *view);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusMetaViewFrame, nautilus_meta_view_frame, NAUTILUS_TYPE_VIEW_FRAME)
     
static void
nautilus_meta_view_frame_initialize_class (NautilusMetaViewFrameClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	NautilusViewFrameClass *view_class;
	
	object_class = (GtkObjectClass*) klass;
	widget_class = (GtkWidgetClass*) klass;
	view_class = (NautilusViewFrameClass*) klass;
	klass->parent_class = gtk_type_class (gtk_type_parent (object_class->type));
	view_class->servant_init_func = POA_Nautilus_MetaViewFrame__init;
	view_class->servant_destroy_func = POA_Nautilus_MetaViewFrame__fini;
	view_class->vepv = &impl_Nautilus_MetaViewFrame_vepv;
}

static void
nautilus_meta_view_frame_initialize (NautilusMetaViewFrame *view)
{
}


void
nautilus_meta_view_frame_set_label (NautilusMetaViewFrame *nview,
                                    const char *label)
{
	nview->label = g_strdup (label);
}


const char *
nautilus_meta_view_frame_get_label(NautilusMetaViewFrame *nview)
{
	return nview->label;
}
