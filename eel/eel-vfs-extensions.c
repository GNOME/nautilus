/* eel-vfs-extensions.c - gnome-vfs extensions.  Its likely some of these will
 *                         be part of gnome-vfs in the future.
 *
 *  Copyright (C) 1999, 2000 Eazel, Inc.
 *
 *  The Gnome Library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Gnome Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with the Gnome Library; see the file COPYING.LIB.  If not,
 *  see <http://www.gnu.org/licenses/>.
 *
 *  Authors: Darin Adler <darin@eazel.com>
 *           Pavel Cisler <pavel@eazel.com>
 *           Mike Fleming  <mfleming@eazel.com>
 *           John Sullivan <sullivan@eazel.com>
 */

#include <config.h>
#include "eel-vfs-extensions.h"
#include "eel-glib-extensions.h"
#include "eel-lib-self-check-functions.h"

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>

#include "eel-string.h"

#include <string.h>
#include <stdlib.h>
