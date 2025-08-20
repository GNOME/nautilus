/* nautilus-localsearch-utilities.h
 *
 * Copyright 2019 Carlos Soriano <csoriano@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
