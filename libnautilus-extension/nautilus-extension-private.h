/*
 * nautilus-extension-private.h - Type definitions for Nautilus extensions
 *
 * SPDX-FileCopyrightText: 2009 Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#pragma once

#include <libnautilus-extension/nautilus-file-info.h>

G_BEGIN_DECLS

extern NautilusFileInfo *(*nautilus_file_info_getter) (GFile *location, gboolean create);

G_END_DECLS
