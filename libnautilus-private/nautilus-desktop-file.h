/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus desktop file handling
 *
 * Copyright (C) 2001 Red Hat, Inc.
 *
 * Author: Havoc Pennington <hp@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef NAUTILUS_DESKTOP_FILE_H
#define NAUTILUS_DESKTOP_FILE_H

#include <libgnome/gnome-defs.h>

BEGIN_GNOME_DECLS

void  nautilus_desktop_file_launch   (const char *uri);
char* nautilus_desktop_file_get_icon (const char *uri);
char* nautilus_desktop_file_get_name (const char *uri);

END_GNOME_DECLS

#endif /* NAUTILUS_DESKTOP_FILE_H */
