/*
 * SPDX-FileCopyrightText: 1999, 2000, 2001 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Pavel Cisler <pavel@eazel.com>
 */

#pragma once

#include <gdk/gdk.h>
#include <gio/gio.h>

void nautilus_file_changes_queue_file_added                      (GFile      *location);
void nautilus_file_changes_queue_file_changed                    (GFile      *location);
void nautilus_file_changes_queue_file_unmounted                  (GFile      *location);
void nautilus_file_changes_queue_file_removed                    (GFile      *location);
void nautilus_file_changes_queue_file_moved                      (GFile      *from,
								  GFile      *to);

void nautilus_file_changes_consume_changes                       (void);
