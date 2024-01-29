/* nautilus-starred-directory.h
 *
 * Copyright (C) 2017 Alexandru Pandelea <alexandru.pandelea@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "nautilus-directory.h"

G_BEGIN_DECLS

#define NAUTILUS_STARRED_DIRECTORY_PROVIDER_NAME "starred-directory-provider"

#define NAUTILUS_TYPE_STARRED_DIRECTORY (nautilus_starred_directory_get_type ())

G_DECLARE_FINAL_TYPE (NautilusFavoriteDirectory, nautilus_starred_directory, NAUTILUS, STARRED_DIRECTORY, NautilusDirectory);

NautilusFavoriteDirectory* nautilus_starred_directory_new      (void);

G_END_DECLS
