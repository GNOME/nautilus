/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/* nautilus-directory-metafile-monitor.h
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

#ifndef NAUTILUS_METAFILE_MONITOR_H
#define NAUTILUS_METAFILE_MONITOR_H

#include "nautilus-metafile-server.h"

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-xobject.h>
#include <libnautilus-private/nautilus-directory.h>

#define NAUTILUS_TYPE_METAFILE_MONITOR	          (nautilus_metafile_monitor_get_type ())
#define NAUTILUS_METAFILE_MONITOR(obj)	          (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_METAFILE_MONITOR, NautilusMetafileMonitor))
#define NAUTILUS_METAFILE_MONITOR_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_METAFILE_MONITOR, NautilusMetafileMonitorClass))
#define NAUTILUS_IS_METAFILE_MONITOR(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_METAFILE_MONITOR))
#define NAUTILUS_IS_METAFILE_MONITOR_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_METAFILE_MONITOR))

typedef struct NautilusMetafileMonitorDetails NautilusMetafileMonitorDetails;

typedef struct {
	BonoboXObject parent_slot;
	NautilusMetafileMonitorDetails *details;
} NautilusMetafileMonitor;

typedef struct {
	BonoboXObjectClass parent_slot;
	POA_Nautilus_MetafileMonitor__epv epv;
} NautilusMetafileMonitorClass;

GtkType nautilus_metafile_monitor_get_type (void);


NautilusMetafileMonitor *nautilus_metafile_monitor_new (NautilusDirectory *directory);

#endif /* NAUTILUS_METAFILE_MONITOR_H */
