/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus
 * Copyright (C) 2000 Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 *
 * This is the header file for the service registration stuff
 *
 */

#ifndef __EAZEL_REGISTER_H__
#define __EAZEL_REGISTER_H__

#include <gnome-xml/entities.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/tree.h>

xmlDoc* create_configuration_metafile(void);
xmlDoc* synchronize_configuration_metafile(void);
xmlDoc* update_configuration_metafile(void);

#endif /* __NAUTILUS_ZOOM_CONTROL_H__ */
