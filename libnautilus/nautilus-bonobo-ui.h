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

/* Components can use these paths with BonoboUIHandler calls to
 * locate menus and menu items for the purpose of menu merging.
 * Note: Not all Nautilus menu items are necessarily published
 * here; these are the ones whose existence components can count on.
 */

/* File menu */
#define NAUTILUS_MENU_PATH_FILE_MENU			"/File"
#define NAUTILUS_MENU_PATH_NEW_WINDOW_ITEM		"/File/New Window"
#define NAUTILUS_MENU_PATH_CLOSE_ITEM			"/File/Close"
#define NAUTILUS_MENU_PATH_CLOSE_ALL_WINDOWS_ITEM	"/File/Close All Windows"

/* Edit menu */
#define NAUTILUS_MENU_PATH_EDIT_MENU			"/Edit"
#define NAUTILUS_MENU_PATH_UNDO_ITEM			"/Edit/Undo"
#define NAUTILUS_MENU_PATH_SEPARATOR_AFTER_UNDO		"/Edit/Separator after Undo"
#define NAUTILUS_MENU_PATH_CUT_ITEM			"/Edit/Cut"
#define NAUTILUS_MENU_PATH_COPY_ITEM			"/Edit/Copy"
#define NAUTILUS_MENU_PATH_PASTE_ITEM			"/Edit/Paste"
#define NAUTILUS_MENU_PATH_CLEAR_ITEM			"/Edit/Clear"
#define NAUTILUS_MENU_PATH_SEPARATOR_AFTER_CLEAR	"/Edit/Separator after Clear"
#define NAUTILUS_MENU_PATH_SELECT_ALL_ITEM		"/Edit/Select All"
#define NAUTILUS_MENU_PATH_SEPARATOR_AFTER_SELECT_ALL	"/Edit/Separator after Select All"

/* Go menu */
#define NAUTILUS_MENU_PATH_GO_MENU			"/Go"
#define NAUTILUS_MENU_PATH_BACK_ITEM			"/Go/Back"
#define NAUTILUS_MENU_PATH_FORWARD_ITEM			"/Go/Forward"
#define NAUTILUS_MENU_PATH_UP_ITEM			"/Go/Up"
#define NAUTILUS_MENU_PATH_HOME_ITEM			"/Go/Home"
#define NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_HISTORY	"/Go/Separator before History"

/* Bookmarks menu */
#define NAUTILUS_MENU_PATH_BOOKMARKS_MENU		"/Bookmarks"
#define NAUTILUS_MENU_PATH_ADD_BOOKMARK_ITEM		"/Bookmarks/Add Bookmark"
#define NAUTILUS_MENU_PATH_EDIT_BOOKMARKS_ITEM		"/Bookmarks/Edit Bookmarks"
#define NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_BOOKMARKS	"/Bookmarks/Separator before Bookmarks"

/* Settings menu */
#define NAUTILUS_MENU_PATH_SETTINGS_MENU		"/Settings"
#define NAUTILUS_MENU_PATH_SEPARATOR_AFTER_USER_LEVELS	"/Settings/Separator after User Levels"

/* Help menu */
#define NAUTILUS_MENU_PATH_HELP_MENU			"/Help"
#define NAUTILUS_MENU_PATH_ABOUT_ITEM			"/Help/About Nautilus"
#define NAUTILUS_MENU_PATH_FEEDBACK_ITEM		"/Help/Nautilus Feedback"
#define NAUTILUS_MENU_PATH_HELP_INFO_ITEM		"/Help/Nautilus Info"

/* Components can use these paths with BonoboUIHandler calls to
 * locate toolbars and toolbar items for the purpose of merging.
 * Note: Not all Nautilus toolbars or toolbar items are necessarily published
 * here; these are the ones whose existence components can count on.
 */

/* Main toolbar */
#define NAUTILUS_TOOLBAR_PATH_MAIN_TOOLBAR		"/Main"

#endif /* NAUTILUS_BONOBO_UI_H */
