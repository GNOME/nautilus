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

/* nautilus-adapter-load-strategy.h
 */

#ifndef NAUTILUS_ADAPTER_LOAD_STRATEGY_H
#define NAUTILUS_ADAPTER_LOAD_STRATEGY_H

#include <gtk/gtkobject.h>
#include <bonobo/Bonobo.h>
#include <libnautilus/nautilus-view.h>

#define NAUTILUS_TYPE_ADAPTER_LOAD_STRATEGY	       (nautilus_adapter_load_strategy_get_type ())
#define NAUTILUS_ADAPTER_LOAD_STRATEGY(obj)	       (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_ADAPTER_LOAD_STRATEGY, NautilusAdapterLoadStrategy))
#define NAUTILUS_ADAPTER_LOAD_STRATEGY_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ADAPTER_LOAD_STRATEGY, NautilusAdapterLoadStrategyClass))
#define NAUTILUS_IS_ADAPTER_LOAD_STRATEGY(obj)	       (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_ADAPTER_LOAD_STRATEGY))
#define NAUTILUS_IS_ADAPTER_LOAD_STRATEGY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_ADAPTER_LOAD_STRATEGY))

typedef struct NautilusAdapterLoadStrategyDetails NautilusAdapterLoadStrategyDetails;

typedef struct {
	GtkObject parent;
} NautilusAdapterLoadStrategy;

typedef struct {
	GtkObjectClass parent;

	/* signals */
        void (* report_load_underway)          (NautilusAdapterLoadStrategy *strategy);
        void (* report_load_progress)          (NautilusAdapterLoadStrategy *strategy,
                                                double fraction_done);
        void (* report_load_complete)          (NautilusAdapterLoadStrategy *strategy);
        void (* report_load_failed)            (NautilusAdapterLoadStrategy *strategy);

	/* virtual methods */
	void  (*load_location) (NautilusAdapterLoadStrategy *strategy,
				const char                  *uri);

	void  (*stop_loading)  (NautilusAdapterLoadStrategy *strategy);
} NautilusAdapterLoadStrategyClass;

/* GtkObject support */
GtkType                      nautilus_adapter_load_strategy_get_type              (void);

/* Instantiates the proper concrete subclass */
NautilusAdapterLoadStrategy *nautilus_adapter_load_strategy_get                   (Bonobo_Unknown  component);

void                         nautilus_adapter_load_strategy_load_location         (NautilusAdapterLoadStrategy *strategy,
										   const char                  *uri);
void                         nautilus_adapter_load_strategy_stop_loading          (NautilusAdapterLoadStrategy *strategy);


/* "protected" calls, should only be called by subclasses */

void                         nautilus_adapter_load_strategy_report_load_underway  (NautilusAdapterLoadStrategy *strategy);
void                         nautilus_adapter_load_strategy_report_load_progress  (NautilusAdapterLoadStrategy *strategy,
										   double                       fraction_done);
void                         nautilus_adapter_load_strategy_report_load_complete  (NautilusAdapterLoadStrategy *strategy);
void                         nautilus_adapter_load_strategy_report_load_failed    (NautilusAdapterLoadStrategy *strategy);


#endif /* NAUTILUS_ADAPTER_LOAD_STRATEGY_H */



