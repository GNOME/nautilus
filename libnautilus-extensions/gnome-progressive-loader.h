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

#ifndef __BONOBO_PROGRESSIVE_LOADER_H__
#define __BONOBO_PROGRESSIVE_LOADER_H__

#include <gnome.h>
#include <bonobo.h>
#include <libgnomevfs/gnome-vfs.h>

#include "Bonobo_ProgressiveLoader.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define GNOME_TYPE_PROGRESSIVE_LOADER \
	(bonobo_progressive_loader_get_type ())
#define BONOBO_PROGRESSIVE_LOADER(obj) \
	(GTK_CHECK_CAST ((obj), GNOME_TYPE_PROGRESSIVE_LOADER, BonoboProgressiveLoader))
#define BONOBO_PROGRESSIVE_LOADER_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_PROGRESSIVE_LOADER, BonoboProgressiveLoaderClass))
#define BONOBO_IS_PROGRESSIVE_LOADER(obj) \
        (GTK_CHECK_TYPE ((obj), GNOME_TYPE_PROGRESSIVE_LOADER))
#define BONOBO_IS_PROGRESSIVE_LOADER_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((obj), GNOME_TYPE_PROGRESSIVE_LOADER))


typedef struct _BonoboProgressiveLoader       BonoboProgressiveLoader;
typedef struct _BonoboProgressiveLoaderClass  BonoboProgressiveLoaderClass;

typedef GnomeVFSResult (* BonoboProgressiveLoaderLoadFn)
			(BonoboProgressiveLoader *progressive_loader,
			 const gchar *uri,
			 Bonobo_ProgressiveDataSink pdsink);

struct _BonoboProgressiveLoader {
	BonoboObject parent;

	BonoboProgressiveLoaderLoadFn load_fn;
};

struct _BonoboProgressiveLoaderClass {
	BonoboObjectClass parent_class;
};


GtkType bonobo_progressive_loader_get_type (void);
BonoboProgressiveLoader *bonobo_progressive_loader_new (BonoboProgressiveLoaderLoadFn fn);
gboolean bonobo_progressive_loader_construct (BonoboProgressiveLoader *loader,
					     Bonobo_ProgressiveLoader corba_loader,
					     BonoboProgressiveLoaderLoadFn load_fn);


extern POA_Bonobo_ProgressiveLoader__epv bonobo_progressive_loader_epv;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __BONOBO_PROGRESSIVE_LOADER_H__ */
