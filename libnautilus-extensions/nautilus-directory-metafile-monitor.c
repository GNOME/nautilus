/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/* nautilus-directory-metafile-monitor.c
 *
 * Copyright (C) 2001 Eazel, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "nautilus-directory-metafile-monitor.h"
#include "nautilus-metafile-server.h"

#include "nautilus-directory-private.h"
#include "nautilus-file-private.h"

#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus/nautilus-bonobo-workarounds.h>

struct NautilusMetafileMonitorDetails {
	NautilusDirectory *directory;
};

static void nautilus_metafile_monitor_initialize       (NautilusMetafileMonitor      *monitor);
static void nautilus_metafile_monitor_initialize_class (NautilusMetafileMonitorClass *klass);

static void destroy (GtkObject *monitor);

static void corba_metafile_changed (PortableServer_Servant       servant,
				    const Nautilus_FileNameList *file_names,
				    CORBA_Environment           *ev);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusMetafileMonitor, nautilus_metafile_monitor, BONOBO_OBJECT_TYPE)

static void
nautilus_metafile_monitor_initialize_class (NautilusMetafileMonitorClass *klass)
{
	GTK_OBJECT_CLASS (klass)->destroy = destroy;
}

static POA_Nautilus_MetafileMonitor__epv *
nautilus_metafile_monitor_get_epv (void)
{
	static POA_Nautilus_MetafileMonitor__epv epv;

	epv.metafile_changed = corba_metafile_changed;
	
	return &epv;
}

static POA_Nautilus_MetafileMonitor__vepv *
nautilus_metafile_monitor_get_vepv (void)
{
	static POA_Nautilus_MetafileMonitor__vepv vepv;

	vepv.Bonobo_Unknown_epv = nautilus_bonobo_object_get_epv ();
	vepv.Nautilus_MetafileMonitor_epv = nautilus_metafile_monitor_get_epv ();

	return &vepv;
}

static POA_Nautilus_MetafileMonitor *
nautilus_metafile_monitor_create_servant (void)
{
	POA_Nautilus_MetafileMonitor *servant;
	CORBA_Environment ev;

	servant = (POA_Nautilus_MetafileMonitor *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = nautilus_metafile_monitor_get_vepv ();
	CORBA_exception_init (&ev);
	POA_Nautilus_MetafileMonitor__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		g_error ("can't initialize Nautilus metafile monitor");
	}
	CORBA_exception_free (&ev);

	return servant;
}

static void
nautilus_metafile_monitor_initialize (NautilusMetafileMonitor *monitor)
{
	Nautilus_MetafileMonitor corba_monitor;

	monitor->details = g_new0 (NautilusMetafileMonitorDetails, 1);

	corba_monitor = bonobo_object_activate_servant
		(BONOBO_OBJECT (monitor), nautilus_metafile_monitor_create_servant ());
	bonobo_object_construct (BONOBO_OBJECT (monitor), corba_monitor);
}

static void
destroy (GtkObject *object)
{
	NautilusMetafileMonitor *monitor;

	monitor = NAUTILUS_METAFILE_MONITOR (object);
	g_free (monitor->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

NautilusMetafileMonitor *
nautilus_metafile_monitor_new (NautilusDirectory *directory)
{
	NautilusMetafileMonitor *monitor;
	monitor = NAUTILUS_METAFILE_MONITOR (gtk_object_new (NAUTILUS_TYPE_METAFILE_MONITOR, NULL));
	monitor->details->directory = directory;
	/* The monitor is owned by the directory, so we don't ref the directory. */
	return monitor;
}

static void
corba_metafile_changed (PortableServer_Servant       servant,
			const Nautilus_FileNameList *file_names,
			CORBA_Environment           *ev)
{
	GList                   *file_list;
	NautilusFile		*file;
	CORBA_unsigned_long      buf_pos;
	NautilusMetafileMonitor *monitor;
	
	monitor = NAUTILUS_METAFILE_MONITOR (bonobo_object_from_servant (servant));

	file_list = NULL;
	
	for (buf_pos = 0; buf_pos < file_names->_length; ++buf_pos) {
		file = nautilus_directory_find_file_by_relative_uri
			(monitor->details->directory, file_names->_buffer [buf_pos]);

		if (file != NULL) {
			if (nautilus_file_is_self_owned (file)) {
				nautilus_file_emit_changed (file);
			} else {
				file_list = g_list_prepend (file_list, file);
			}
		}
	}

	if (file_list != NULL) {
		file_list = g_list_reverse (file_list);
		nautilus_directory_emit_change_signals (monitor->details->directory, file_list);
		g_list_free (file_list);
	}
}
