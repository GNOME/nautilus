/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-preferences-private.h - Private preferences functions

   Copyright (C) 2000 Eazel, Inc.

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

   Authors: George Lebl <jirka@5z.com>
*/

#ifndef NAUTILUS_PREFERENCES_PRIVATE_H
#define NAUTILUS_PREFERENCES_PRIVATE_H

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

BEGIN_GNOME_DECLS

/* returns FALSE if no error, and TRUE if an error was handeled */ 
gboolean    nautilus_preferences_handle_error      (GError      **error);


END_GNOME_DECLS

#endif /* NAUTILUS_PREFERENCES_PRIVATE_H */
