/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 */


/* nautilus-adapter-progressive-load-strategy.c - 
 */


#include <config.h>

#include "nautilus-adapter-progressive-load-strategy.h"

#include <gtk/gtkobject.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libgnomevfs/gnome-vfs.h>
#include <bonobo/Bonobo.h>

#include <stdio.h>

struct NautilusAdapterProgressiveLoadStrategyDetails {
	Bonobo_ProgressiveDataSink  progressive_data_sink;
	gboolean                    stop;
};


static void nautilus_adapter_progressive_load_strategy_initialize_class (NautilusAdapterProgressiveLoadStrategyClass *klass);
static void nautilus_adapter_progressive_load_strategy_initialize       (NautilusAdapterProgressiveLoadStrategy      *strategy);
static void nautilus_adapter_progressive_load_strategy_destroy          (GtkObject                              *object);

static void nautilus_adapter_progressive_load_strategy_load_location (NautilusAdapterLoadStrategy *strategy,
								 const char                  *uri);

static void nautilus_adapter_progressive_load_strategy_stop_loading  (NautilusAdapterLoadStrategy *strategy);


NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusAdapterProgressiveLoadStrategy, nautilus_adapter_progressive_load_strategy, NAUTILUS_TYPE_ADAPTER_LOAD_STRATEGY)


static void
nautilus_adapter_progressive_load_strategy_initialize_class (NautilusAdapterProgressiveLoadStrategyClass *klass)
{
	GtkObjectClass                   *object_class;
	NautilusAdapterLoadStrategyClass *adapter_load_strategy_class;

	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = nautilus_adapter_progressive_load_strategy_destroy;

	adapter_load_strategy_class = NAUTILUS_ADAPTER_LOAD_STRATEGY_CLASS (klass);

	adapter_load_strategy_class->load_location = nautilus_adapter_progressive_load_strategy_load_location;
	adapter_load_strategy_class->stop_loading = nautilus_adapter_progressive_load_strategy_stop_loading;
}

static void
nautilus_adapter_progressive_load_strategy_initialize (NautilusAdapterProgressiveLoadStrategy *strategy)
{
	strategy->details = g_new0 (NautilusAdapterProgressiveLoadStrategyDetails, 1);
}

static void
nautilus_adapter_progressive_load_strategy_destroy (GtkObject *object)
{
	NautilusAdapterProgressiveLoadStrategy *strategy;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	strategy = NAUTILUS_ADAPTER_PROGRESSIVE_LOAD_STRATEGY (object);

	if (strategy->details->progressive_data_sink != CORBA_OBJECT_NIL) {
		bonobo_object_release_unref (strategy->details->progressive_data_sink, &ev);
	}

	CORBA_exception_free (&ev);

	g_free (strategy->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

NautilusAdapterLoadStrategy *
nautilus_adapter_progressive_load_strategy_new (Bonobo_ProgressiveDataSink  progressive_data_sink)
{
	NautilusAdapterProgressiveLoadStrategy *strategy;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	strategy = NAUTILUS_ADAPTER_PROGRESSIVE_LOAD_STRATEGY (gtk_object_new (NAUTILUS_TYPE_ADAPTER_PROGRESSIVE_LOAD_STRATEGY, NULL));
	gtk_object_ref (GTK_OBJECT (strategy));
	gtk_object_sink (GTK_OBJECT (strategy));

	strategy->details->progressive_data_sink = progressive_data_sink;

	CORBA_exception_free (&ev);

	return NAUTILUS_ADAPTER_LOAD_STRATEGY (strategy);
}


static void
stop_loading (NautilusAdapterProgressiveLoadStrategy *strategy,
	      GnomeVFSHandle *handle,
	      Bonobo_ProgressiveDataSink_iobuf *iobuf,
	      CORBA_Environment *ev) 
{
	Bonobo_ProgressiveDataSink_end (strategy->details->progressive_data_sink, ev); 
	nautilus_adapter_load_strategy_report_load_failed (NAUTILUS_ADAPTER_LOAD_STRATEGY (strategy));
	gtk_object_unref (GTK_OBJECT (strategy));
	gnome_vfs_close (handle);
	CORBA_free (iobuf);
	CORBA_exception_free (ev);
}



#define STOP_LOADING                                           \
        do {                                                   \
	         stop_loading (strategy, handle, iobuf, &ev);  \
		 return;                                       \
        } while (0)

#define CHECK_IF_STOPPED                                       \
        do {                                                   \
	        if (strategy->details->stop) {                 \
                        STOP_LOADING;                          \
		}                                              \
	} while (0)


#define LOAD_CHUNK 2048

static void
nautilus_adapter_progressive_load_strategy_load_location (NautilusAdapterLoadStrategy *abstract_strategy,
							  const char                  *uri)
{
	NautilusAdapterProgressiveLoadStrategy *strategy;
	GnomeVFSFileInfo  file_info;
	GnomeVFSHandle   *handle;
	GnomeVFSResult    result;
	GnomeVFSFileSize  bytes_read;
	Bonobo_ProgressiveDataSink_iobuf *iobuf;
	CORBA_octet *data;
	CORBA_Environment ev;

	strategy = NAUTILUS_ADAPTER_PROGRESSIVE_LOAD_STRATEGY (abstract_strategy);

	strategy->details->stop = FALSE;
	gtk_object_ref (GTK_OBJECT (strategy));

	CORBA_exception_init (&ev);

	/* FIXME bugzilla.eazel.com 3455: this code is stupid and
           loads the component in a way that blocks the nautilus
           adapter component, which is pointless/stupid; it should be
           async. */

	if (gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_READ) != GNOME_VFS_OK) {
		nautilus_adapter_load_strategy_report_load_failed (abstract_strategy);
		gtk_object_unref (GTK_OBJECT (strategy));
		CORBA_exception_free (&ev);
		return;
	}

	iobuf = Bonobo_ProgressiveDataSink_iobuf__alloc ();
	CORBA_sequence_set_release (iobuf, TRUE);
	data = CORBA_sequence_CORBA_octet_allocbuf (LOAD_CHUNK);
	iobuf->_buffer = data;

	nautilus_adapter_load_strategy_report_load_underway (abstract_strategy);

	if (strategy->details->stop) {
		nautilus_adapter_load_strategy_report_load_failed (abstract_strategy);
		gtk_object_unref (GTK_OBJECT (strategy));
		CORBA_exception_free (&ev);
		return;
	}

	Bonobo_ProgressiveDataSink_start (strategy->details->progressive_data_sink, &ev);

	CHECK_IF_STOPPED;

	gnome_vfs_file_info_init (&file_info);
	result = gnome_vfs_get_file_info_from_handle (handle, &file_info, GNOME_VFS_FILE_INFO_DEFAULT);
	gnome_vfs_file_info_clear (&file_info);

	if (result == GNOME_VFS_OK && file_info.valid_fields | GNOME_VFS_FILE_INFO_FIELDS_SIZE) {
		Bonobo_ProgressiveDataSink_set_size (strategy->details->progressive_data_sink, 
						     (long) file_info.size, &ev);
		CHECK_IF_STOPPED;
	}
		
	do {
		result = gnome_vfs_read (handle, data, LOAD_CHUNK, &bytes_read);

		if (result == GNOME_VFS_OK) {
			iobuf->_length = bytes_read;

			Bonobo_ProgressiveDataSink_add_data (strategy->details->progressive_data_sink, iobuf, &ev);
			
			CHECK_IF_STOPPED;
			
			if (ev._major != CORBA_NO_EXCEPTION) {
				STOP_LOADING;
			}
		} else if (result == GNOME_VFS_ERROR_EOF) {
			if (ev._major == CORBA_NO_EXCEPTION) {
				Bonobo_ProgressiveDataSink_end (strategy->details->progressive_data_sink, &ev);
				nautilus_adapter_load_strategy_report_load_complete (abstract_strategy);
				gtk_object_unref (GTK_OBJECT (strategy));
				gnome_vfs_close (handle);
				CORBA_free (iobuf);
				CORBA_exception_free (&ev);
				return;
			} else {
				STOP_LOADING;
			}
		} else {
			STOP_LOADING;
		}
	} while (TRUE);
}

static void
nautilus_adapter_progressive_load_strategy_stop_loading  (NautilusAdapterLoadStrategy *abstract_strategy)
{
	NautilusAdapterProgressiveLoadStrategy *strategy;

	strategy = NAUTILUS_ADAPTER_PROGRESSIVE_LOAD_STRATEGY (abstract_strategy);

	strategy->details->stop = TRUE;
}

