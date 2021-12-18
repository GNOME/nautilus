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
    NAUTILUS_CANVAS_ICON_SIZE_SMALL    = 48,
    NAUTILUS_CANVAS_ICON_SIZE_STANDARD = 64,
    NAUTILUS_CANVAS_ICON_SIZE_LARGE    = 96,
    NAUTILUS_CANVAS_ICON_SIZE_LARGER   = 128,
    NAUTILUS_CANVAS_ICON_SIZE_LARGEST  = 256,
} NautilusCanvasIconSize;

typedef enum
{
    NAUTILUS_CANVAS_ZOOM_LEVEL_SMALL,
    NAUTILUS_CANVAS_ZOOM_LEVEL_STANDARD,
    NAUTILUS_CANVAS_ZOOM_LEVEL_LARGE,
    NAUTILUS_CANVAS_ZOOM_LEVEL_LARGER,
    NAUTILUS_CANVAS_ZOOM_LEVEL_LARGEST,
} NautilusCanvasZoomLevel;

typedef enum
{
    NAUTILUS_LIST_ICON_SIZE_SMALL    = 16,
    NAUTILUS_LIST_ICON_SIZE_STANDARD = 32,
    NAUTILUS_LIST_ICON_SIZE_LARGE    = 48,
    NAUTILUS_LIST_ICON_SIZE_LARGER   = 64,
} NautilusListIconSize;

typedef enum
{
    NAUTILUS_LIST_ZOOM_LEVEL_SMALL,
    NAUTILUS_LIST_ZOOM_LEVEL_STANDARD,
    NAUTILUS_LIST_ZOOM_LEVEL_LARGE,
    NAUTILUS_LIST_ZOOM_LEVEL_LARGER,
} NautilusListZoomLevel;

typedef enum
{
    NAUTILUS_FILE_ATTRIBUTE_INFO                      = 1 << 0, /* All standard info */
    NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS               = 1 << 1,
    NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT      = 1 << 2,
    NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_MIME_TYPES = 1 << 3,
    NAUTILUS_FILE_ATTRIBUTE_EXTENSION_INFO            = 1 << 4,
    NAUTILUS_FILE_ATTRIBUTE_THUMBNAIL                 = 1 << 5,
    NAUTILUS_FILE_ATTRIBUTE_MOUNT                     = 1 << 6,
    NAUTILUS_FILE_ATTRIBUTE_FILESYSTEM_INFO           = 1 << 7,
} NautilusFileAttributes;

typedef enum
{
    NAUTILUS_OPEN_FLAG_NORMAL           = 1 << 0,
    NAUTILUS_OPEN_FLAG_NEW_WINDOW       = 1 << 1,
    NAUTILUS_OPEN_FLAG_NEW_TAB          = 1 << 2,
    NAUTILUS_OPEN_FLAG_SLOT_APPEND      = 1 << 3,
    NAUTILUS_OPEN_FLAG_DONT_MAKE_ACTIVE = 1 << 4,
} NautilusOpenFlags;
