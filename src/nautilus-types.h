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

/* This here header contains Nautilus type definitions.
 *
 * It is advisable to include this in headers or when you
 * only need the name of a type (i.e. not calling any functions from the
 * associated header). Doing so will help avoid circular inclusions and
 * pointless rebuilds should the header ever change.
 */

#pragma once

#include "nautilus-enums.h"

#include <gmodule.h>

/* Keep sorted alphabetically. */

typedef struct _EelCanvas                     EelCanvas;
typedef struct _EelCanvasItem                 EelCanvasItem;
typedef struct _NautilusBookmark              NautilusBookmark;
typedef struct _NautilusBookmarkList          NautilusBookmarkList;
typedef struct _NautilusCanvasContainer       NautilusCanvasContainer;
typedef struct _NautilusCanvasItem            NautilusCanvasItem;
typedef struct _NautilusCanvasView            NautilusCanvasView;
typedef struct _NautilusDirectory             NautilusDirectory;
typedef struct  NautilusFile                  NautilusFile;
typedef GList                                *NautilusFileList;
typedef struct  NautilusFileQueue             NautilusFileQueue;
typedef struct _NautilusFilesView             NautilusFilesView;
typedef struct _NautilusIconInfo              NautilusIconInfo;
typedef struct  NautilusMonitor               NautilusMonitor;
typedef struct _NautilusQuery                 NautilusQuery;
typedef struct _NautilusQueryEditor           NautilusQueryEditor;
typedef struct _NautilusToolbarMenuSections   NautilusToolbarMenuSections;
typedef struct _NautilusView                  NautilusView;
typedef struct _NautilusWindow                NautilusWindow;
typedef struct _NautilusWindowSlot            NautilusWindowSlot;
