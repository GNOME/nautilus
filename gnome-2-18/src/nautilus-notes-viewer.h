/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000, 2004 Red Hat, Inc.
 *  Copyright (C) 2000, 2001 Eazel, Inc.
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
 *  Authors: Andy Hertzfeld <andy@eazel.com>
 *           Alexander Larsson <alexl@redhat.com>
 */
#ifndef _NAUTILUS_NOTES_VIEWER_H
#define _NAUTILUS_NOTES_VIEWER_H

#include <gtk/gtktreeview.h>
#include <libnautilus-private/nautilus-view.h>
#include <libnautilus-private/nautilus-window-info.h>
#include <gtk/gtkscrolledwindow.h>

#define NAUTILUS_NOTES_SIDEBAR_ID    "NautilusNotesSidebar"

#define NAUTILUS_TYPE_NOTES_VIEWER (nautilus_notes_viewer_get_type ())
#define NAUTILUS_NOTES_VIEWER(obj) (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_NOTES_VIEWER, NautilusNotesViewer))

typedef struct _NautilusNotesViewerDetails NautilusNotesViewerDetails;

typedef struct {
	GtkScrolledWindow parent;
	NautilusNotesViewerDetails *details;
} NautilusNotesViewer;

GType nautilus_notes_viewer_get_type (void);
void nautilus_notes_viewer_register (void);

#endif
