/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *  This is the header file for the index panel widget, which displays overview information
 *  in a vertical panel and hosts the meta-views.
 */

#ifndef NAUTILUS_EMBLEM_VIEW_H
#define NAUTILUS_EMBLEM_VIEW_H

#include <libnautilus/nautilus-view.h>

#define NAUTILUS_TYPE_EMBLEM_VIEW \
	(nautilus_emblem_view_get_type ())
#define NAUTILUS_EMBLEM_VIEW(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_EMBLEM_VIEW, NautilusEmblemView))
#define NAUTILUS_EMBLEM_VIEW_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_EMBLEM_VIEW, NautilusEmblemViewClass))
#define NAUTILUS_IS_EMBLEM_VIEW(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_EMBLEM_VIEW))
#define NAUTILUS_IS_EMBLEM_VIEW_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_EMBLEM_VIEW))

typedef struct NautilusEmblemViewDetails NautilusEmblemViewDetails;

typedef struct {
	NautilusView parent_slot;
	NautilusEmblemViewDetails *details;
} NautilusEmblemView;

typedef struct {
	NautilusViewClass parent_slot;
	
} NautilusEmblemViewClass;

GType	nautilus_emblem_view_get_type     (void);

#endif /* NAUTILUS_EMBLEM_VIEW_H */
