/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Maciej Stachowiak
 */

/* libmain.c - object activation infrastructure for shared library
   version of tree view. */

#include <config.h>

#include "nautilus-tree-view.h"

#include "nautilus-tree-view-iids.h"

#include <liboaf/liboaf.h>
#include <bonobo.h>




static void
tree_shlib_object_destroyed (GtkObject *object)
{
	/* FIXME bugzilla.eazel.com 2736: oaf_plugin_unuse can't possibly work! this sucks */

	oaf_plugin_unuse (gtk_object_get_user_data (object));
}


static CORBA_Object
tree_shlib_make_object (PortableServer_POA poa,
			const char *iid,
			gpointer impl_ptr,
			CORBA_Environment *ev)
{
	NautilusTreeView *view;
	NautilusView *nautilus_view;


	if (strcmp (iid, TREE_VIEW_IID) != 0) {
		return CORBA_OBJECT_NIL;
	}

	view = NAUTILUS_TREE_VIEW (gtk_object_new (NAUTILUS_TYPE_TREE_VIEW, NULL));

	nautilus_view = nautilus_tree_view_get_nautilus_view (view);

	gtk_signal_connect (GTK_OBJECT (nautilus_view), "destroy", tree_shlib_object_destroyed, NULL);

	gtk_object_set_user_data (GTK_OBJECT (nautilus_view), impl_ptr);

	oaf_plugin_use (poa, impl_ptr);

	return CORBA_Object_duplicate (bonobo_object_corba_objref 
				       (BONOBO_OBJECT (nautilus_view)), ev);
}


static const OAFPluginObject tree_plugin_list[] = {
	{
  		TREE_VIEW_IID,
		tree_shlib_make_object
	},
	
  	{
  		NULL
  	}
};

const OAFPlugin OAF_Plugin_info = {
	tree_plugin_list,
	"Nautilus Tree Sidebar Panel"
};

