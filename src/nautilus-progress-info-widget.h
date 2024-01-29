/*
 * nautilus-progress-info-widget.h: file operation progress user interface.
 *
 * Copyright (C) 2007, 2011 Red Hat, Inc.
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#pragma once

#include <gtk/gtk.h>

#include "nautilus-progress-info.h"

#define NAUTILUS_TYPE_PROGRESS_INFO_WIDGET nautilus_progress_info_widget_get_type()
#define NAUTILUS_PROGRESS_INFO_WIDGET(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_PROGRESS_INFO_WIDGET, NautilusProgressInfoWidget))
#define NAUTILUS_PROGRESS_INFO_WIDGET_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PROGRESS_INFO_WIDGET, NautilusProgressInfoWidgetClass))
#define NAUTILUS_IS_PROGRESS_INFO_WIDGET(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_PROGRESS_INFO_WIDGET))
#define NAUTILUS_IS_PROGRESS_INFO_WIDGET_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PROGRESS_INFO_WIDGET))
#define NAUTILUS_PROGRESS_INFO_WIDGET_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_PROGRESS_INFO_WIDGET, NautilusProgressInfoWidgetClass))

typedef struct _NautilusProgressInfoWidgetPrivate NautilusProgressInfoWidgetPrivate;

typedef struct {
	GtkGrid parent;

	/* private */
	NautilusProgressInfoWidgetPrivate *priv;
} NautilusProgressInfoWidget;

typedef struct {
	GtkGridClass parent_class;
} NautilusProgressInfoWidgetClass;

GType nautilus_progress_info_widget_get_type (void);

GtkWidget * nautilus_progress_info_widget_new (NautilusProgressInfo *info);
