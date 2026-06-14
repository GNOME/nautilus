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

#define NAUTILUS_ATTRIBUTE_N_TOTAL 9
typedef enum
{
    /* Some attributes require others, which is done by ORing them */

    NAUTILUS_ATTRIBUTE_INFO                 = 1 << 0, /* All standard info */
    NAUTILUS_ATTRIBUTE_DEEP_COUNT           = 1 << 1,
    NAUTILUS_ATTRIBUTE_DIRECTORY_ITEM_COUNT = 1 << 2,
    NAUTILUS_ATTRIBUTE_THUMBNAIL_INFO       = 1 << 3 | NAUTILUS_ATTRIBUTE_INFO,
    NAUTILUS_ATTRIBUTE_EXTENSION_INFO       = 1 << 4,
    NAUTILUS_ATTRIBUTE_THUMBNAIL_BUFFER     = 1 << 5 | NAUTILUS_ATTRIBUTE_THUMBNAIL_INFO,
    NAUTILUS_ATTRIBUTE_MOUNT                = 1 << 6 | NAUTILUS_ATTRIBUTE_INFO,
    NAUTILUS_ATTRIBUTE_FILESYSTEM_INFO      = 1 << 7,
    NAUTILUS_ATTRIBUTE_FILE_LIST            = 1 << 8,
    /* Adjust NAUTILUS_ATTRIBUTE_N_TOTAL when expanding */
} NautilusAttributes;

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
    NAUTILUS_OPEN_FLAG_REUSE_EXISTING   = 1 << 4,
} NautilusOpenFlags;

/* See org.gnome.nautilus.SearchFilterTimeType schema */
typedef enum {
    NAUTILUS_SEARCH_TIME_TYPE_LAST_MODIFIED = 0,
    NAUTILUS_SEARCH_TIME_TYPE_LAST_ACCESS,
    NAUTILUS_SEARCH_TIME_TYPE_CREATED,
} NautilusSearchTimeType;

typedef enum {
  NAUTILUS_SIDEBAR_ROW_INVALID = 0,
  NAUTILUS_SIDEBAR_ROW_BUILT_IN,
  NAUTILUS_SIDEBAR_ROW_EXTERNAL_MOUNT,
  NAUTILUS_SIDEBAR_ROW_INTERNAL_MOUNT,
  NAUTILUS_SIDEBAR_ROW_NEW_BOOKMARK,
  NAUTILUS_SIDEBAR_ROW_BOOKMARK,
  NAUTILUS_SIDEBAR_ROW_BOOKMARK_PLACEHOLDER,
  NAUTILUS_SIDEBAR_N_ROW_TYPES
} NautilusSidebarRowType;

/* Keep order, since it's used for the sort functions */
typedef enum {
  NAUTILUS_SIDEBAR_SECTION_INVALID = 0,
  NAUTILUS_SIDEBAR_SECTION_DEFAULT_LOCATIONS,
  NAUTILUS_SIDEBAR_SECTION_BOOKMARKS,
  NAUTILUS_SIDEBAR_SECTION_CLOUD,
  NAUTILUS_SIDEBAR_SECTION_MOUNTS,
  NAUTILUS_SIDEBAR_N_SECTION_TYPES
} NautilusSidebarSectionType;

/**
 * NautilusSelectionSource:
 * @NAUTILUS_SELECTION_SOURCE_NONE: Fallback for undefined source.
 * @NAUTILUS_SELECTION_SOURCE_MANUAL: Selection was manual by the user through
 * manual clicking, rubberbanding, pattern selection, or selection shortcuts.
 * @NAUTILUS_SELECTION_SOURCE_AUTO: Selection was programmatically by a source
 * other than what is defined below.
 * @NAUTILUS_SELECTION_SOURCE_IN_SEARCH: Automatic selection of the first item
 * during search
 * @NAUTILUS_SELECTION_SOURCE_AFTER_SEARCH: Same as IN_SEARCH, but transferred
 * after exiting search without change.
 * @NAUTILUS_SELECTION_SOURCE_OP_START: The selection that existed at the time
 * of an operation start.
 * @NAUTILUS_SELECTION_SOURCE_OP_DONE: Selection that appears as a result of an
 * operation after it has finished.
 *
 **/
typedef enum
{
    NAUTILUS_SELECTION_SOURCE_NONE,
    NAUTILUS_SELECTION_SOURCE_MANUAL,
    NAUTILUS_SELECTION_SOURCE_AUTO,
    NAUTILUS_SELECTION_SOURCE_IN_SEARCH,
    NAUTILUS_SELECTION_SOURCE_AFTER_SEARCH,
    NAUTILUS_SELECTION_SOURCE_OP_START,
    NAUTILUS_SELECTION_SOURCE_OP_DONE,
} NautilusSelectionSource;
