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
 * Components can use these menu paths with BonoboUIHandler calls to 
 * place entire new menus. 
 */

#define NAUTILUS_MENU_PATH_FILE_MENU			"/File"
#define NAUTILUS_MENU_PATH_EDIT_MENU			"/Edit"
#define NAUTILUS_MENU_PATH_GO_MENU			"/Go"
#define NAUTILUS_MENU_PATH_BOOKMARKS_MENU		"/Bookmarks"
#define NAUTILUS_MENU_PATH_HELP_MENU			"/Help"

/** 
 * Components can use these menu item paths with BonoboUIHandler calls to 
 * merge over certain existing items. Only items that we expect to be
 * merged over are listed here, to avoid making public details that might
 * change later. 
 */

#define NAUTILUS_MENU_PATH_CUT_ITEM			"/Edit/Cut"
#define NAUTILUS_MENU_PATH_COPY_ITEM			"/Edit/Copy"
#define NAUTILUS_MENU_PATH_PASTE_ITEM			"/Edit/Paste"
#define NAUTILUS_MENU_PATH_CLEAR_ITEM			"/Edit/Clear"
#define NAUTILUS_MENU_PATH_SELECT_ALL_ITEM		"/Edit/Select All"

/** 
 * Components can use these placeholder paths with BonoboUIHandler calls to 
 * insert new items in well-defined positions. 
 */

/* Use the "new items" placeholder to insert menu items like "New xxx" */
#define NAUTILUS_MENU_PATH_NEW_ITEMS_PLACEHOLDER	"/File/New Items Placeholder"

/**
 * Use the "open" placeholder to insert menu items dealing with opening the
 * selected item, like "Open", "Open in New Window", etc.
 */
#define NAUTILUS_MENU_PATH_OPEN_PLACEHOLDER		"/File/Open Placeholder"

/**
 * Use the "file items" placeholder to insert other File menu items dealing with
 * individual files, such as "Show Properties" and "Rename"
 */
#define NAUTILUS_MENU_PATH_FILE_ITEMS_PLACEHOLDER	"/File/File Items Placeholder"

/**
 * Use the "global file items" placeholder to insert other File menu items 
 * dealing with nautilus as a whole, such as "Empty Trash".
 */
#define NAUTILUS_MENU_PATH_GLOBAL_FILE_ITEMS_PLACEHOLDER "/File/Global File Items Placeholder"

/**
 * Use the "global edit items" placeholder to insert other Edit menu items 
 * dealing with nautilus as a whole, such as "Icon Captions...".
 */
#define NAUTILUS_MENU_PATH_GLOBAL_EDIT_ITEMS_PLACEHOLDER "/Edit/Global Edit Items Placeholder"

/**
 * Use the "edit items" placeholder to insert other Edit menu items dealing with
 * individual files, such as "Remove Custom Image"
 */
#define NAUTILUS_MENU_PATH_EDIT_ITEMS_PLACEHOLDER	"/Edit/Edit Items Placeholder"

/* Use the "extra help items" placeholder to add help-related items */
#define NAUTILUS_MENU_PATH_EXTRA_HELP_ITEMS_PLACEHOLDER	"/Help/Extra Help Items"


/* Components can use these paths with BonoboUIHandler calls to
 * locate toolbars and toolbar items for the purpose of merging.
 * Note: Not all Nautilus toolbars or toolbar items are necessarily published
 * here; these are the ones whose existence components can count on.
 */

/* Main toolbar */
#define NAUTILUS_TOOLBAR_PATH_MAIN_TOOLBAR		"/Main"

#endif /* NAUTILUS_BONOBO_UI_H */
