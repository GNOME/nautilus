/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-scroll-positionable.h - public interface for objects that implement
 *                                  scroll positioning
 *
 * Copyright (C) 2003 Red Hat, Inc.
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
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#ifndef NAUTILUS_SCROLL_POSITIONABLE_H
#define NAUTILUS_SCROLL_POSITIONABLE_H

#include <libnautilus/nautilus-view-component.h>
#include <bonobo/bonobo-object.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SCROLL_POSITIONABLE        (nautilus_scroll_positionable_get_type ())
#define NAUTILUS_SCROLL_POSITIONABLE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_SCROLL_POSITIONABLE, NautilusScrollPositionable))
#define NAUTILUS_SCROLL_POSITIONABLE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_SCROLL_POSITIONABLE, NautilusScrollPositionableClass))
#define BONOBO_IS_POSITIONABLE(o)                (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_SCROLL_POSITIONABLE))
#define BONOBO_IS_POSITIONABLE_CLASS(k)          (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_SCROLL_POSITIONABLE))
#define NAUTILUS_SCROLL_POSITIONABLE_GET_CLASS(o)(G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_SCROLL_POSITIONABLE, NautilusScrollPositionableClass))

typedef struct _NautilusScrollPositionablePrivate	NautilusScrollPositionablePrivate;

typedef struct {
        BonoboObject		object;
} NautilusScrollPositionable;

typedef struct {
	BonoboObjectClass	parent;
	POA_Nautilus_ScrollPositionable__epv epv;

	char * (*get_first_visible_file)  (NautilusScrollPositionable *positionable);
	void   (*scroll_to_file)	  (NautilusScrollPositionable *positionable,
					   const char                 *uri);

	gpointer dummy[4];
} NautilusScrollPositionableClass;

GType		 nautilus_scroll_positionable_get_type                       (void) G_GNUC_CONST;

NautilusScrollPositionable	*nautilus_scroll_positionable_new	(void);

#endif /* NAUTILUS_SCROLL_POSITIONABLE_H */
