/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000 Eazel, Inc
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
 * Author: Maciej Stachowiak <mjs@eazel.com>
 */

/* nautilus-adapter-embeddable-embed-strategy.h
 */

#ifndef NAUTILUS_ADAPTER_EMBEDDABLE_EMBED_STRATEGY_H
#define NAUTILUS_ADAPTER_EMBEDDABLE_EMBED_STRATEGY_H

#include "nautilus-adapter-embed-strategy.h"

#define NAUTILUS_TYPE_ADAPTER_EMBEDDABLE_EMBED_STRATEGY	      (nautilus_adapter_embeddable_embed_strategy_get_type ())
#define NAUTILUS_ADAPTER_EMBEDDABLE_EMBED_STRATEGY(obj)	      (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_ADAPTER_EMBEDDABLE_EMBED_STRATEGY, NautilusAdapterEmbeddableEmbedStrategy))
#define NAUTILUS_ADAPTER_EMBEDDABLE_EMBED_STRATEGY_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ADAPTER_EMBEDDABLE_EMBED_STRATEGY, NautilusAdapterEmbeddableEmbedStrategyClass))
#define NAUTILUS_IS_ADAPTER_EMBEDDABLE_EMBED_STRATEGY(obj)	      (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_ADAPTER_EMBEDDABLE_EMBED_STRATEGY))
#define NAUTILUS_IS_ADAPTER_EMBEDDABLE_EMBED_STRATEGY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_ADAPTER_EMBEDDABLE_EMBED_STRATEGY))

typedef struct NautilusAdapterEmbeddableEmbedStrategyDetails NautilusAdapterEmbeddableEmbedStrategyDetails;

typedef struct {
	NautilusAdapterEmbedStrategy parent;
	NautilusAdapterEmbeddableEmbedStrategyDetails *details;
} NautilusAdapterEmbeddableEmbedStrategy;

typedef struct {
	NautilusAdapterEmbedStrategyClass parent;
} NautilusAdapterEmbeddableEmbedStrategyClass;

/* GtkObject support */
GtkType                             nautilus_adapter_embeddable_embed_strategy_get_type (void);

NautilusAdapterEmbedStrategy       *nautilus_adapter_embeddable_embed_strategy_new      (Bonobo_Embeddable  embeddable,
											 Bonobo_UIContainer ui_container);


#endif /* NAUTILUS_ADAPTER_EMBEDDABLE_EMBED_STRATEGY_H */



