/* Copyright (C) 2018 Ernestas Kulik <ernestask@gnome.org>
 *
 * This file is part of Nautilus.
 *
 * Nautilus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nautilus.  If not, see <https://www.gnu.org/licenses/>.
 */

/* This is the little brother of nautilus-types.h and only contains enumerations.
 *
 * Now that you’ve familiarized yourself with it, the reason for its existence
 * is similar, and the split is purely for convenience reasons. Include this
 * when you only need a certain enumeration and not the whole header that might
 * have had it originally. Otherwise, include both!
 */

#pragma once

/* Keep sorted alphabetically. */

typedef enum
{
    NAUTILUS_GRID_ICON_SIZE_SMALL       = 48,
    NAUTILUS_GRID_ICON_SIZE_SMALL_PLUS  = 64,
    NAUTILUS_GRID_ICON_SIZE_MEDIUM      = 96,
    NAUTILUS_GRID_ICON_SIZE_LARGE       = 168,
    NAUTILUS_GRID_ICON_SIZE_EXTRA_LARGE = 256,
} NautilusGridIconSize;

typedef enum
{
    NAUTILUS_GRID_ZOOM_LEVEL_SMALL,
    NAUTILUS_GRID_ZOOM_LEVEL_SMALL_PLUS,
    NAUTILUS_GRID_ZOOM_LEVEL_MEDIUM,
    NAUTILUS_GRID_ZOOM_LEVEL_LARGE,
    NAUTILUS_GRID_ZOOM_LEVEL_EXTRA_LARGE,
} NautilusGridZoomLevel;

typedef enum
{
    NAUTILUS_LIST_ICON_SIZE_SMALL  = 16,
    NAUTILUS_LIST_ICON_SIZE_MEDIUM = 32,
    NAUTILUS_LIST_ICON_SIZE_LARGE  = 64,
} NautilusListIconSize;

typedef enum
{
    NAUTILUS_LIST_ZOOM_LEVEL_SMALL,
    NAUTILUS_LIST_ZOOM_LEVEL_MEDIUM,
    NAUTILUS_LIST_ZOOM_LEVEL_LARGE,
} NautilusListZoomLevel;

typedef enum
{
    NAUTILUS_FILE_ATTRIBUTE_INFO                      = 1 << 0, /* All standard info */
    NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS               = 1 << 1,
    NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT      = 1 << 2,
    NAUTILUS_FILE_ATTRIBUTE_THUMBNAIL_INFO            = 1 << 3,
    NAUTILUS_FILE_ATTRIBUTE_EXTENSION_INFO            = 1 << 4,
    NAUTILUS_FILE_ATTRIBUTE_THUMBNAIL_BUFFER          = 1 << 5,
    NAUTILUS_FILE_ATTRIBUTE_MOUNT                     = 1 << 6,
    NAUTILUS_FILE_ATTRIBUTE_FILESYSTEM_INFO           = 1 << 7,
} NautilusFileAttributes;

typedef enum
{
    NAUTILUS_MODE_BROWSE = 0,
    NAUTILUS_MODE_OPEN_FILE,
    NAUTILUS_MODE_OPEN_FILES,
    NAUTILUS_MODE_OPEN_FOLDER,
    NAUTILUS_MODE_OPEN_FOLDERS,
    NAUTILUS_MODE_SAVE_FILE,
    NAUTILUS_MODE_SAVE_FILES,
} NautilusMode;

typedef enum
{
    NAUTILUS_OPEN_FLAG_NORMAL           = 1 << 0,
    NAUTILUS_OPEN_FLAG_NEW_WINDOW       = 1 << 1,
    NAUTILUS_OPEN_FLAG_NEW_TAB          = 1 << 2,
    NAUTILUS_OPEN_FLAG_DONT_MAKE_ACTIVE = 1 << 3,
} NautilusOpenFlags;
