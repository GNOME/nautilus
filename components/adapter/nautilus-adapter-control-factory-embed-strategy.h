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

/* nautilus-adapter-control_factory-embed-strategy.h
 */

#ifndef NAUTILUS_ADAPTER_CONTROL_FACTORY_EMBED_STRATEGY_H
#define NAUTILUS_ADAPTER_CONTROL_FACTORY_EMBED_STRATEGY_H

#include "nautilus-adapter-embed-strategy.h"

#define NAUTILUS_TYPE_ADAPTER_CONTROL_FACTORY_EMBED_STRATEGY		\
	(nautilus_adapter_control_factory_embed_strategy_get_type ())
#define NAUTILUS_ADAPTER_CONTROL_FACTORY_EMBED_STRATEGY(obj)		\
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_ADAPTER_CONTROL_FACTORY_EMBED_STRATEGY, NautilusAdapterControlFactoryEmbedStrategy))
#define NAUTILUS_ADAPTER_CONTROL_FACTORY_EMBED_STRATEGY_CLASS(klass)	\
	(G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ADAPTER_CONTROL_FACTORY_EMBED_STRATEGY, NautilusAdapterControlFactoryEmbedStrategyClass))
#define NAUTILUS_IS_ADAPTER_CONTROL_FACTORY_EMBED_STRATEGY(obj)		\
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_ADAPTER_CONTROL_FACTORY_EMBED_STRATEGY))
#define NAUTILUS_IS_ADAPTER_CONTROL_FACTORY_EMBED_STRATEGY_CLASS(klass)	\
	(G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_ADAPTER_CONTROL_FACTORY_EMBED_STRATEGY))

typedef struct NautilusAdapterControlFactoryEmbedStrategyDetails NautilusAdapterControlFactoryEmbedStrategyDetails;

typedef struct {
	NautilusAdapterEmbedStrategy parent;
	NautilusAdapterControlFactoryEmbedStrategyDetails *details;
} NautilusAdapterControlFactoryEmbedStrategy;

typedef struct {
	NautilusAdapterEmbedStrategyClass parent;
} NautilusAdapterControlFactoryEmbedStrategyClass;

/* GObject support */
GType                          nautilus_adapter_control_factory_embed_strategy_get_type (void);

NautilusAdapterEmbedStrategy  *nautilus_adapter_control_factory_embed_strategy_new      (Bonobo_ControlFactory  control_factory,
											 Bonobo_UIContainer ui_container);

#endif /* NAUTILUS_ADAPTER_CONTROL_FACTORY_EMBED_STRATEGY_H */
