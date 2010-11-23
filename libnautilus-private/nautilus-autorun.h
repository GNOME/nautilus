/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2008 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

/* TODO:
 *
 * - finish x-content / * types
 *  - finalize the semi-spec
 *  - add probing/sniffing code
 * - clean up code
 * - implement missing features
 *  - Autorun spec (e.g. $ROOT/.autostart)
 *
 */

#ifndef NAUTILUS_AUTORUN_H
#define NAUTILUS_AUTORUN_H

#include <gtk/gtk.h>
#include <libnautilus-private/nautilus-file.h>

typedef void (*NautilusAutorunGetContent) (char **content, gpointer user_data);

char **nautilus_autorun_get_cached_x_content_types_for_mount (GMount       *mount);

void nautilus_autorun_get_x_content_types_for_mount_async (GMount *mount,
							   NautilusAutorunGetContent callback,
							   GCancellable *cancellable,
							   gpointer user_data);

void nautilus_autorun_launch_for_mount (GMount *mount, GAppInfo *app_info);

#endif /* NAUTILUS_AUTORUN_H */
