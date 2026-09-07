/*
 * SPDX-FileCopyrightText: 2019 Carlos Soriano <csoriano@redhat.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#pragma once

#include <gio/gio.h>
#include <tinysparql.h>

TrackerSparqlConnection * nautilus_localsearch_get_miner_fs_connection (GError **error);
void nautilus_localsearch_setup_miner_fs_connection (void);

/* nautilus_localsearch_setup_host_miner_fs_connection_sync() is for testing purposes only */
void nautilus_localsearch_setup_host_miner_fs_connection_sync (void);

gboolean nautilus_localsearch_directory_is_tracked (GFile *directory);
gboolean nautilus_localsearch_directory_is_single (GFile *directory);
