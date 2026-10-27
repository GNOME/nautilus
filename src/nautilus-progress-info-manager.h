/*
 * SPDX-FileCopyrightText: 2011 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 */

#pragma once

#include "nautilus-types.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_PROGRESS_INFO_MANAGER nautilus_progress_info_manager_get_type()
G_DECLARE_FINAL_TYPE (NautilusProgressInfoManager, nautilus_progress_info_manager, NAUTILUS, PROGRESS_INFO_MANAGER, GObject)

NautilusProgressInfoManager* nautilus_progress_info_manager_dup_singleton (void);

void nautilus_progress_info_manager_add_new_info (NautilusProgressInfoManager *self,
                                                  NautilusProgressInfo *info);
GList *nautilus_progress_info_manager_get_all_infos (NautilusProgressInfoManager *self);

G_END_DECLS
