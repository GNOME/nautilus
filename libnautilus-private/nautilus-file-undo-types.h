/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-undo-types.h - Data structures used by undo/redo
 *
 * Copyright (C) 2007-2011 Amos Brocco
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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
 * Authors: Amos Brocco <amos.brocco@gmail.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __NAUTILUS_FILE_UNDO_TYPES_H__
#define __NAUTILUS_FILE_UNDO_TYPES_H__

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

typedef struct {
	const char *undo_label;
	const char *undo_description;
	const char *redo_label;
	const char *redo_description;
} NautilusFileUndoMenuData;

typedef struct _NautilusFileUndoData NautilusFileUndoData;
typedef struct _NautilusFileUndoManager NautilusFileUndoManager;
typedef void (* NautilusFileUndoFinishCallback) (NautilusFileUndoData *data,
						 gboolean success,
						 gpointer user_data);

struct _NautilusFileUndoManagerPrivate
{
	GQueue *stack;

	/* Used to protect access to stack (because of async file ops) */
	GMutex mutex;

	guint undo_levels;
	guint index;
	guint undo_redo_flag : 1;
};

#endif /* __NAUTILUS_FILE_UNDO_TYPES_H__ */
