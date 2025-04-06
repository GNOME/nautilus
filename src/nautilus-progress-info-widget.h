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
G_DECLARE_FINAL_TYPE (NautilusProgressInfoWidget, nautilus_progress_info_widget, NAUTILUS, PROGRESS_INFO_WIDGET, GtkGrid)

GtkWidget * nautilus_progress_info_widget_new (NautilusProgressInfo *info);
