/*
 * SPDX-FileCopyrightText: 2018 Ernestas Kulik <ernestask@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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

#include <glib.h>

/* Keep sorted alphabetically. */

typedef struct _NautilusBatchRenameDialog   NautilusBatchRenameDialog;
typedef struct _NautilusBookmark            NautilusBookmark;
typedef struct _NautilusBookmarkList        NautilusBookmarkList;
typedef struct _NautilusClipboard           NautilusClipboard;
typedef struct _NautilusColumn              NautilusColumn;
typedef struct _NautilusDirectory           NautilusDirectory;
typedef struct  NautilusFile                NautilusFile;
typedef struct _NautilusFileOperationsDBusData NautilusFileOperationsDBusData;
typedef struct _NautilusFileUndoInfo        NautilusFileUndoInfo;
typedef struct _NautilusFilesView           NautilusFilesView;
typedef struct _NautilusFileUndoManager     NautilusFileUndoManager;
typedef struct  NautilusHashQueue           NautilusHashQueue;
typedef struct _NautilusIconInfo            NautilusIconInfo;
typedef struct _NautilusListBase            NautilusListBase;
typedef struct  NautilusMonitor             NautilusMonitor;
typedef struct _NautilusProgressInfo        NautilusProgressInfo;
typedef struct _NautilusQuery               NautilusQuery;
typedef struct _NautilusQueryEditor         NautilusQueryEditor;
typedef struct _NautilusSearchHit           NautilusSearchHit;
typedef struct _NautilusSearchProvider      NautilusSearchProvider;
typedef struct _NautilusViewCell            NautilusViewCell;
typedef struct _NautilusViewItem            NautilusViewItem;
typedef struct _NautilusViewModel           NautilusViewModel;
typedef struct _NautilusWindow              NautilusWindow;
typedef struct _NautilusWindowSlot          NautilusWindowSlot;

/* List aliases used for implicit type documentation */
#define LIST_ALIAS(ALIAS) \
    typedef GList ALIAS; \
    G_DEFINE_AUTOPTR_CLEANUP_FUNC(ALIAS, g_list_free)

LIST_ALIAS(GFileList)
LIST_ALIAS(NautilusDirectoryList)
LIST_ALIAS(NautilusFileList)
