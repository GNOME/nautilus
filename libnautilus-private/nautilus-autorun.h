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
 * - automount all user-visible media on startup
 *  - but avoid doing autorun for these
 * - unmount all the media we've automounted on shutdown
 * - finish x-content / * types
 *  - finalize the semi-spec
 *  - add probing/sniffing code
 * - clean up code
 * - implement missing features
 *  - "Open Folder when mounted"
 *  - Autorun spec (e.g. $ROOT/.autostart)
 *
 */

#ifndef NAUTILUS_AUTORUN_H
#define NAUTILUS_AUTORUN_H

#include <gtk/gtkvbox.h>
#include <eel/eel-background.h>
#include <libnautilus-private/nautilus-file.h>

void _g_mount_guess_content_type_async (GMount              *mount,
					gboolean             force_rescan,
					GCancellable        *cancellable,
					GAsyncReadyCallback  callback,
					gpointer             user_data);

char ** _g_mount_guess_content_type_finish (GMount              *mount,
					    GAsyncResult        *result,
					    GError             **error);

char ** _g_mount_guess_content_type (GMount              *mount,
				     gboolean             force_rescan,
				     GError             **error);


typedef void (*NautilusAutorunComboBoxChanged) (gboolean selected_ask,
						gboolean selected_ignore,
						gboolean selected_open_folder,
						GAppInfo *selected_app,
						gpointer user_data);

typedef void (*NautilusAutorunOpenWindow) (GMount *mount, gpointer user_data);
typedef void (*NautilusAutorunGetContent) (char **content, gpointer user_data);

void nautilus_autorun_prepare_combo_box (GtkWidget *combo_box, 
					 const char *x_content_type, 
					 gboolean include_ask,
					 gboolean update_settings,
					 NautilusAutorunComboBoxChanged changed_cb,
					 gpointer user_data);

void nautilus_autorun_set_preferences (const char *x_content_type, gboolean pref_ask, gboolean pref_ignore, gboolean pref_open_folder);
void nautilus_autorun_get_preferences (const char *x_content_type, gboolean *pref_ask, gboolean *pref_ignore, gboolean *pref_open_folder);

void nautilus_autorun (GMount *mount, NautilusAutorunOpenWindow open_window_func, gpointer user_data);

char **nautilus_autorun_get_cached_x_content_types_for_mount (GMount       *mount);

void nautilus_autorun_get_x_content_types_for_mount_async (GMount *mount,
							   NautilusAutorunGetContent callback,
							   GCancellable *cancellable,
							   gpointer user_data);

void nautilus_autorun_launch_for_mount (GMount *mount, GAppInfo *app_info);

void nautilus_allow_autorun_for_volume (GVolume *volume);
void nautilus_allow_autorun_for_volume_finish (GVolume *volume);

#endif /* NAUTILUS_AUTORUN_H */
