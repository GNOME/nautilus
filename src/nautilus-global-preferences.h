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

BEGIN_GNOME_DECLS

#define NAUTILUS_PREFS_WINDOW_ALWAYS_NEW	"/nautilus/prefs/window_always_new"
#define NAUTILUS_PREFS_WINDOW_SEARCH_EXISTING	"/nautilus/prefs/window_search_existing"
#define NAUTILUS_PREFS_USER_LEVEL		"/nautilus/prefs/user_level"

enum
{
	NAUTILUS_USER_LEVEL_NOVICE,
	NAUTILUS_USER_LEVEL_INTERMEDIATE,
	NAUTILUS_USER_LEVEL_HACKER,
	NAUTILUS_USER_LEVEL_ETTORE
};

void           nautilus_prefs_global_shutdown    (void);
void           nautilus_prefs_global_show_dialog (void);
NautilusPrefs *nautilus_prefs_global_get_prefs   (void);

BEGIN_GNOME_DECLS

#endif /* NAUTILUS_PREFS_GLOBAL_H */


