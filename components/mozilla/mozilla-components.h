/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
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
 *  Author: Ramiro Estrugo <ramiro@eazel.com>
 *
 */

/*
 * mozilla-components.h - A small C wrapper for using mozilla components
 */

#ifndef MOZILLA_COMPONENTS_H
#define MOZILLA_COMPONENTS_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

gboolean mozilla_components_register_library (const char *class_uuid,
					      const char *library_file_name,
					      const char *class_name,
					      const char *prog_id);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MOZILLA_COMPONENTS_H */
