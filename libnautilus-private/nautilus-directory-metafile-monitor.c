/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

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

#include "nautilus-directory-private.h"
#include "nautilus-file-private.h"

#include <eel/eel-gtk-macros.h>
#include <eel/eel-glib-extensions.h>

struct NautilusMetafileMonitorDetails {
	NautilusDirectory *directory;
};

BONOBO_CLASS_BOILERPLATE_FULL (NautilusMetafileMonitor, nautilus_metafile_monitor,
			       Nautilus_MetafileMonitor,
			       BonoboObject, BONOBO_OBJECT_TYPE)

static void
nautilus_metafile_monitor_instance_init (NautilusMetafileMonitor *monitor)
{
	monitor->details = g_new0 (NautilusMetafileMonitorDetails, 1);
}

static void
finalize (GObject *object)
{
	NautilusMetafileMonitor *monitor;

	monitor = NAUTILUS_METAFILE_MONITOR (object);

	g_free (monitor->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

NautilusMetafileMonitor *
nautilus_metafile_monitor_new (NautilusDirectory *directory)
{
	NautilusMetafileMonitor *monitor;
	
	monitor = NAUTILUS_METAFILE_MONITOR (g_object_new (NAUTILUS_TYPE_METAFILE_MONITOR, NULL));
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
		file = nautilus_directory_find_file_by_internal_filename
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

static void
corba_metafile_ready (PortableServer_Servant       servant,
		      CORBA_Environment           *ev)
{
	NautilusMetafileMonitor *monitor;

	monitor = NAUTILUS_METAFILE_MONITOR (bonobo_object_from_servant (servant));
	emit_change_signals_for_all_files (monitor->details->directory);
	nautilus_idle_queue_add (monitor->details->directory->details->idle_queue,
				 (GFunc) nautilus_directory_async_state_changed,
				 monitor->details->directory,
				 NULL,
				 NULL);
}

static void
nautilus_metafile_monitor_class_init (NautilusMetafileMonitorClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = finalize;

	klass->epv.metafile_changed = corba_metafile_changed;
	klass->epv.metafile_ready   = corba_metafile_ready;
}
