/*
 * nautilus-directory-notify.h: Nautilus directory notify calls.
 *
 * SPDX-FileCopyrightText: 2000, 2001 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Darin Adler <darin@bentspoon.com>
 */

#pragma once

#include <gio/gio.h>

#include "nautilus-types.h"

typedef struct {
	GFile *from;
	GFile *to;
} GFilePair;

/* Almost-public change notification calls */
void nautilus_directory_notify_files_added   (GList *files);
void nautilus_directory_notify_files_moved   (GList *file_pairs);
void nautilus_directory_notify_files_changed (GList *files);
void nautilus_directory_notify_files_removed (GList *files);

/* Unmount state hack.
 * This must be called right before nautilus_directory_notify_files_removed(),
 * to ensure that, when the file is notified as gone, it already knows it was
 * due to an unmount event. */
void nautilus_directory_mark_files_unmounted (GList *files);

/* Change notification hack.
 * This is called when code modifies the file and it needs to trigger
 * a notification. Eventually this should become private, but for now
 * it needs to be used for code like the thumbnail generation.
 */
void nautilus_file_changed                       (NautilusFile *file);
