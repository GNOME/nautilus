/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/*
 *  libnautilus: A library for nautilus view implementations.
 *
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Maciej Stachowiak <mjs@eazel.com>
 *
 */

/* nautilus-bonobo-ui.h: bonobo UI paths usable by Nautilus components 
 * for merging menus and toolbars. 
 */

#ifndef NAUTILUS_BONOBO_UI_H
#define NAUTILUS_BONOBO_UI_H

/** 
 * Components can use these menu paths with BonoboUIContainer calls to 
 * place entire new menus. 
 */

#define NAUTILUS_MENU_PATH_FILE_MENU			"/menu/File"
#define NAUTILUS_MENU_PATH_EDIT_MENU			"/menu/Edit"
#define NAUTILUS_MENU_PATH_VIEW_MENU			"/menu/View"
#define NAUTILUS_MENU_PATH_GO_MENU			"/menu/Go"
#define NAUTILUS_MENU_PATH_BOOKMARKS_MENU		"/menu/Bookmarks"
#define NAUTILUS_MENU_PATH_PROFILER			"/menu/Profiler"
#define NAUTILUS_MENU_PATH_HELP_MENU			"/menu/Help"

/** 
 * Components can use these menu item paths with BonoboUIContainer calls to 
 * merge over certain existing items. Only items that we expect to be
 * merged over are listed here, to avoid making public details that might
 * change later. 
 */

#define NAUTILUS_MENU_PATH_CUT_ITEM			"/menu/Edit/Cut"
#define NAUTILUS_MENU_PATH_COPY_ITEM			"/menu/Edit/Copy"
#define NAUTILUS_MENU_PATH_PASTE_ITEM			"/menu/Edit/Paste"
#define NAUTILUS_MENU_PATH_CLEAR_ITEM			"/menu/Edit/Clear"
#define NAUTILUS_MENU_PATH_SELECT_ALL_ITEM		"/menu/Edit/Select All"

#define NAUTILUS_COMMAND_CUT				"/commands/Cut"
#define NAUTILUS_COMMAND_COPY				"/commands/Copy"
#define NAUTILUS_COMMAND_PASTE				"/commands/Paste"
#define NAUTILUS_COMMAND_CLEAR				"/commands/Clear"
#define NAUTILUS_COMMAND_SELECT_ALL			"/commands/Select All"

/** 
 * Components can use these placeholder paths with BonoboUIContainer calls to 
 * insert new items in well-defined positions. 
 */

/* Use the "new items" placeholder to insert menu items like "New xxx" */
#define NAUTILUS_MENU_PATH_NEW_ITEMS_PLACEHOLDER	"/menu/File/New Items Placeholder"

/**
 * Use the "open" placeholder to insert menu items dealing with opening the
 * selected item, like "Open", "Open in New Window", etc.
 */
#define NAUTILUS_MENU_PATH_OPEN_PLACEHOLDER		"/menu/File/Open Placeholder"

/**
 * Use the "file items" placeholder to insert other File menu items dealing with
 * individual files, such as "Show Properties" and "Rename"
 */
#define NAUTILUS_MENU_PATH_FILE_ITEMS_PLACEHOLDER	"/menu/File/File Items Placeholder"

/**
 * Use the "global file items" placeholder to insert other File menu items 
 * dealing with nautilus as a whole, such as "Empty Trash".
 */
#define NAUTILUS_MENU_PATH_GLOBAL_FILE_ITEMS_PLACEHOLDER "/menu/File/Global File Items Placeholder"

/**
 * Use the "global edit items" placeholder to insert other Edit menu items 
 * dealing with nautilus as a whole, such as "Icon Captions...".
 */
#define NAUTILUS_MENU_PATH_GLOBAL_EDIT_ITEMS_PLACEHOLDER "/menu/Edit/Global Edit Items Placeholder"

/**
 * Use the "edit items" placeholder to insert other Edit menu items dealing with
 * individual files, such as "Remove Custom Image"
 */
#define NAUTILUS_MENU_PATH_EDIT_ITEMS_PLACEHOLDER	"/menu/Edit/Edit Items Placeholder"

/**
 * Use the "show/hide" placeholder to insert other View menu items that
 * control the visibility of some piece of the UI, such as "Show/Hide Status Bar".
 */
#define NAUTILUS_MENU_PATH_SHOW_HIDE_PLACEHOLDER	"/menu/View/Show Hide Placeholder"

/**
 * Use the "view items" placeholder to insert other View menu items that
 * are specific to a component, such as the Icon View's layout options.
 */
#define NAUTILUS_MENU_PATH_VIEW_ITEMS_PLACEHOLDER	"/menu/View/View Items Placeholder"

/* Use the "extra help items" placeholder to add help-related items */
#define NAUTILUS_MENU_PATH_EXTRA_HELP_ITEMS_PLACEHOLDER	"/menu/Help/Extra Help Items"

/* This holds the zooming-related items in the context menu */
#define NAUTILUS_POPUP_PATH_ZOOM_ITEMS_PLACEHOLDER	"/popups/background/Zoom Items"

/* Components can use these paths with BonoboUIHandler calls to
 * locate toolbars and toolbar items for the purpose of merging.
 * Note: Not all Nautilus toolbars or toolbar items are necessarily published
 * here; these are the ones whose existence components can count on.
 */

/* Main toolbar */
#define NAUTILUS_TOOLBAR_PATH_MAIN_TOOLBAR		"/Main"

#endif /* NAUTILUS_BONOBO_UI_H */
