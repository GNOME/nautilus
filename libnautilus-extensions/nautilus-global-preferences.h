/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-global-prefs.h - Nautilus main preferences api.

   Copyright (C) 1999, 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef NAUTILUS_PREFS_GLOBAL_H
#define NAUTILUS_PREFS_GLOBAL_H

#include <gnome.h>
#include <nautilus-widgets/nautilus-preferences.h>
#include <libnautilus-extensions/nautilus-string-list.h>

BEGIN_GNOME_DECLS

/* Window options */
#define NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW			"/nautilus/preferences/window_always_new"
#define NAUTILUS_PREFERENCES_WINDOW_SEARCH_EXISTING		"/nautilus/preferences/window_search_existing"

/* Show hidden files  */
#define NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES  		"/nautilus/preferences/show_hidden_files"

/* Home URI  */
#define NAUTILUS_PREFERENCES_HOME_URI                 		"/nautilus/preferences/home_uri"

/* Wellknown meta views */
#define NAUTILUS_PREFERENCES_META_VIEWS_SHOW_ANNOTATIONS	"/nautilus/metaviews/ntl_notes_view"
#define NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_CONTENTS	"/nautilus/metaviews/hyperbola_navigation_tree"
#define NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_INDEX		"/nautilus/metaviews/hyperbola_navigation_index"
#define NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_SEARCH	"/nautilus/metaviews/hyperbola_navigation_search"
#define NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HISTORY		"/nautilus/metaviews/ntl_history_view"
#define NAUTILUS_PREFERENCES_META_VIEWS_SHOW_WEB_SEARCH		"/nautilus/metaviews/ntl_websearch_view"

/* Preferences not (currently?) displayed in dialog */
#define NAUTILUS_PREFERENCES_ICON_VIEW_TEXT_ATTRIBUTE_NAMES	"/nautilus/icon_view/text_attribute_names"
#define NAUTILUS_PREFERENCES_ICON_THEME				"/nautilus/preferences/icon_theme"

/* Single/Double click preference  */
#define NAUTILUS_PREFERENCES_CLICK_POLICY			"/nautilus/preferences/click_policy"

enum
{
	NAUTILUS_CLICK_POLICY_SINGLE,
	NAUTILUS_CLICK_POLICY_DOUBLE
};

void                      nautilus_global_preferences_startup            (void);
void                      nautilus_global_preferences_shutdown           (void);
void                      nautilus_global_preferences_show_dialog        (void);
const NautilusStringList *nautilus_global_preferences_get_meta_view_iids (void);

BEGIN_GNOME_DECLS

#endif /* NAUTILUS_PREFS_GLOBAL_H */


