/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-progressive-loader.h
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

/* FIXME this is just a quick hack.  */

#ifndef __GNOME_PROGRESSIVE_LOADER_H__
#define __GNOME_PROGRESSIVE_LOADER_H__

#include <gnome.h>
#include <bonobo/gnome-bonobo.h>
#include <libgnomevfs/gnome-vfs.h>

#include "GNOME_ProgressiveLoader.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define GNOME_TYPE_PROGRESSIVE_LOADER \
	(gnome_progressive_loader_get_type ())
#define GNOME_PROGRESSIVE_LOADER(obj) \
	(GTK_CHECK_CAST ((obj), GNOME_TYPE_PROGRESSIVE_LOADER, GnomeProgressiveLoader))
#define GNOME_PROGRESSIVE_LOADER_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_PROGRESSIVE_LOADER, GnomeProgressiveLoaderClass))
#define GNOME_IS_PROGRESSIVE_LOADER(obj) \
        (GTK_CHECK_TYPE ((obj), GNOME_TYPE_PROGRESSIVE_LOADER))
#define GNOME_IS_PROGRESSIVE_LOADER_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((obj), GNOME_TYPE_PROGRESSIVE_LOADER))


typedef struct _GnomeProgressiveLoader       GnomeProgressiveLoader;
typedef struct _GnomeProgressiveLoaderClass  GnomeProgressiveLoaderClass;

typedef GnomeVFSResult (* GnomeProgressiveLoaderLoadFn)
			(GnomeProgressiveLoader *progressive_loader,
			 const gchar *uri,
			 GNOME_ProgressiveDataSink pdsink);

struct _GnomeProgressiveLoader {
	GnomeObject parent;

	GnomeProgressiveLoaderLoadFn load_fn;
};

struct _GnomeProgressiveLoaderClass {
	GnomeObjectClass parent_class;
};


GtkType gnome_progressive_loader_get_type (void);
GnomeProgressiveLoader *gnome_progressive_loader_new (GnomeProgressiveLoaderLoadFn fn);
gboolean gnome_progressive_loader_construct (GnomeProgressiveLoader *loader,
					     GNOME_ProgressiveLoader corba_loader,
					     GnomeProgressiveLoaderLoadFn load_fn);


extern POA_GNOME_ProgressiveLoader__epv gnome_progressive_loader_epv;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GNOME_PROGRESSIVE_LOADER_H__ */
