/*
 *  Nautilus SendTo extension
 *
 *  Copyright (C) 2005 Roberto Majadas
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 *  Author: Roberto Majadas <roberto.majadas@openshine.com>
 *
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_NSTE (nautilus_nste_get_type ())

G_DECLARE_FINAL_TYPE (NautilusNste, nautilus_nste, NAUTILUS, NSTE, GObject)

void nautilus_nste_load (GTypeModule *module);

G_END_DECLS