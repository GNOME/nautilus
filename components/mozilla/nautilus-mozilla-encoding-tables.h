/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Ramiro Estrugo <ramiro@eazel.com>
 *
 */

/*
 * nautilus-mozilla-encoding-tables.h - Tables for obtaining translated
 * labels for Mozilla charset decoders and charset groups.
 */

#ifndef NAUTILUS_MOZILLA_ENCODING_TABLES_H
#define NAUTILUS_MOZILLA_ENCODING_TABLES_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

guint       mozilla_encoding_table_get_count                  (void);
const char *mozilla_encoding_table_peek_nth                   (guint       n);
const char *mozilla_encoding_table_peek_nth_translated        (guint       n);
const char *mozilla_encoding_table_find_translated            (const char *encoding);
guint       mozilla_encoding_groups_table_get_count           (void);
const char *mozilla_encoding_groups_table_peek_nth            (guint       n);
const char *mozilla_encoding_groups_table_peek_nth_translated (guint       n);
const char *mozilla_encoding_groups_table_find_translated     (const char *encoding_group);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NAUTILUS_MOZILLA_ENCODING_TABLES_H */
