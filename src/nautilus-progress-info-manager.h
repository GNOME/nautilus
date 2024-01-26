/*
 * Nautilus
 *
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 */

#pragma once

#include <glib-object.h>

#include "nautilus-progress-info.h"

#define OPERATION_MINIMUM_TIME 2 /*s */

#define NAUTILUS_TYPE_PROGRESS_INFO_MANAGER nautilus_progress_info_manager_get_type()
G_DECLARE_FINAL_TYPE (NautilusProgressInfoManager, nautilus_progress_info_manager, NAUTILUS, PROGRESS_INFO_MANAGER, GObject)

NautilusProgressInfoManager* nautilus_progress_info_manager_dup_singleton (void);

void nautilus_progress_info_manager_add_new_info (NautilusProgressInfoManager *self,
                                                  NautilusProgressInfo *info);
GList *nautilus_progress_info_manager_get_all_infos (NautilusProgressInfoManager *self);

G_END_DECLS
