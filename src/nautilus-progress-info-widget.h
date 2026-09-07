/*
 * nautilus-progress-info-widget.h: file operation progress user interface.
 *
 * SPDX-FileCopyrightText: 2007, 2011 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 */

#pragma once

#include "nautilus-types.h"

#include <gtk/gtk.h>

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
