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

/* FIXME: Remove next line with gobject-linter version > 0.1.2 available */
/* gobject-linter-ignore-next-line: missing_implementation */
extern NautilusFileInfo *(*nautilus_file_info_getter) (GFile *location, gboolean create);

G_END_DECLS
