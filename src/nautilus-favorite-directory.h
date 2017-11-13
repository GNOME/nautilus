/* nautilus-favorite-directory.h
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

#ifndef NAUTILUS_FAVORITE_DIRECTORY_H
#define NAUTILUS_FAVORITE_DIRECTORY_H

#include "nautilus-directory.h"

G_BEGIN_DECLS

#define NAUTILUS_FAVORITE_DIRECTORY_PROVIDER_NAME "favorite-directory-provider"

#define NAUTILUS_TYPE_FAVORITE_DIRECTORY (nautilus_favorite_directory_get_type ())

G_DECLARE_FINAL_TYPE (NautilusFavoriteDirectory, nautilus_favorite_directory, NAUTILUS, FAVORITE_DIRECTORY, NautilusDirectory);

NautilusFavoriteDirectory* nautilus_favorite_directory_new      ();

G_END_DECLS

#endif
