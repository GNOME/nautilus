/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/* nautilus-metafile.h - server side of Nautilus::MetafileFactory
 *
 * Copyright (C) 2001 Eazel, Inc.
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

#ifndef NAUTILUS_METAFILE_FACTORY_H
#define NAUTILUS_METAFILE_FACTORY_H

#include <libnautilus-private/nautilus-metafile-server.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-xobject.h>

#define NAUTILUS_TYPE_METAFILE_FACTORY	          (nautilus_metafile_factory_get_type ())
#define NAUTILUS_METAFILE_FACTORY(obj)	          (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_METAFILE_FACTORY, NautilusMetafileFactory))
#define NAUTILUS_METAFILE_FACTORY_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_METAFILE_FACTORY, NautilusMetafileFactoryClass))
#define NAUTILUS_IS_METAFILE_FACTORY(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_METAFILE_FACTORY))
#define NAUTILUS_IS_METAFILE_FACTORY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_METAFILE_FACTORY))

typedef struct NautilusMetafileFactoryDetails NautilusMetafileFactoryDetails;

typedef struct {
	BonoboXObject parent_slot;
	NautilusMetafileFactoryDetails *details;
} NautilusMetafileFactory;

typedef struct {
	BonoboXObjectClass parent_slot;
	POA_Nautilus_MetafileFactory__epv epv;
} NautilusMetafileFactoryClass;

GtkType nautilus_metafile_factory_get_type (void);


#define METAFILE_FACTORY_IID "OAFIID:nautilus_metafile_factory:bc318c01-4106-4622-8d6c-b419ec89e4c6"

NautilusMetafileFactory *nautilus_metafile_factory_get_instance (void);

#endif /* NAUTILUS_METAFILE_FACTORY_H */
