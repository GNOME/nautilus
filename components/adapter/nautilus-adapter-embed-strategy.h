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

/* nautilus-adapter-embed-strategy.h
 */

#ifndef NAUTILUS_ADAPTER_EMBED_STRATEGY_H
#define NAUTILUS_ADAPTER_EMBED_STRATEGY_H

#include <gtk/gtkobject.h>
#include <bonobo/Bonobo.h>
#include <libnautilus/nautilus-view.h>

#define NAUTILUS_TYPE_ADAPTER_EMBED_STRATEGY	       (nautilus_adapter_embed_strategy_get_type ())
#define NAUTILUS_ADAPTER_EMBED_STRATEGY(obj)	       (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_ADAPTER_EMBED_STRATEGY, NautilusAdapterEmbedStrategy))
#define NAUTILUS_ADAPTER_EMBED_STRATEGY_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ADAPTER_EMBED_STRATEGY, NautilusAdapterEmbedStrategyClass))
#define NAUTILUS_IS_ADAPTER_EMBED_STRATEGY(obj)	       (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_ADAPTER_EMBED_STRATEGY))
#define NAUTILUS_IS_ADAPTER_EMBED_STRATEGY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_ADAPTER_EMBED_STRATEGY))

typedef struct NautilusAdapterEmbedStrategyDetails NautilusAdapterEmbedStrategyDetails;

typedef struct {
	GtkObject parent;
} NautilusAdapterEmbedStrategy;

typedef struct {
	GtkObjectClass parent;

	/* signals */
	void       (*activate)       (NautilusAdapterEmbedStrategy *strategy,
				      gpointer                      corba_container);
	void       (*deactivate)     (NautilusAdapterEmbedStrategy *strategy);
	void       (*open_location)  (NautilusAdapterEmbedStrategy *strategy,
				      const char                   *uri);

	/* virtual functions */
	GtkWidget    *(*get_widget)  (NautilusAdapterEmbedStrategy *strategy);
	BonoboObject *(*get_zoomable)(NautilusAdapterEmbedStrategy *strategy);

} NautilusAdapterEmbedStrategyClass;

/* GtkObject support */
GtkType                      nautilus_adapter_embed_strategy_get_type      (void);

/* Instantiates the proper concrete subclass */
NautilusAdapterEmbedStrategy *nautilus_adapter_embed_strategy_get       (Bonobo_Unknown     component);

void                          nautilus_adapter_embed_strategy_activate  (NautilusAdapterEmbedStrategy *strategy,
									 Bonobo_UIContainer            ui_container);
void                          nautilus_adapter_embed_strategy_deactivate(NautilusAdapterEmbedStrategy *strategy);

GtkWidget                   *nautilus_adapter_embed_strategy_get_widget (NautilusAdapterEmbedStrategy *strategy);

BonoboObject                *nautilus_adapter_embed_strategy_get_zoomable (NautilusAdapterEmbedStrategy *strategy);


#endif /* NAUTILUS_ADAPTER_EMBED_STRATEGY_H */



