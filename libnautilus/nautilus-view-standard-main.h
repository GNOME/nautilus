/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/*
 *  libnautilus: A library for nautilus view implementations.
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
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

/* nautilus-view-standard-main.h - An implementation of most of a
 * typical main.c file for Nautilus views. Just call the function from
 * main and pass it the right arguments. This should make writing
 * Nautilus views simpler.
 */

#ifndef NAUTILUS_VIEW_STANDARD_MAIN_H
#define NAUTILUS_VIEW_STANDARD_MAIN_H

#include <libnautilus/nautilus-view.h>

BEGIN_GNOME_DECLS

typedef NautilusView * (*NautilusViewCreateFunction) (const char *iid, void *user_data);

int nautilus_view_standard_main        (const char                 *executable_name,
					const char                 *version,
					const char                 *gettext_package_name,
					const char                 *gettext_locale_directory,
					int                         argc,
					char                      **argv,
					const char                 *factory_iid,
					const char                 *view_iid,
					NautilusViewCreateFunction  create_function,
					GVoidFunc                   post_initialize_callback,
					void                       *user_data);

int nautilus_view_standard_main_multi  (const char                 *executable_name,
					const char                 *version,
					const char                 *gettext_package_name,
					const char                 *gettext_locale_directory,
					int                         argc,
					char                      **argv,
					const char                 *factory_iid,
					GList                      *view_iids,       /* GList<const char *> */
					NautilusViewCreateFunction  create_function,
					GVoidFunc                   post_initialize_callback,
					void                       *user_data);

/* standard handy create function (pass the _get_type function for the
 * class as the user_data)
 */
NautilusView * nautilus_view_create_from_get_type_function (const char *iid, void *user_data);

END_GNOME_DECLS

#endif /* NAUTILUS_VIEW_STANDARD_MAIN_H */
