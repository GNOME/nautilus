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
 * nautilus-mozilla-embed-extensions.h - Extensions to GtkMozEmbed.
 */

#ifndef NAUTILUS_MOZILLA_EMBED_EXTENSIONS_H
#define NAUTILUS_MOZILLA_EMBED_EXTENSIONS_H

#include <glib.h>
#include "gtkmozembed.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

gboolean mozilla_charset_set_encoding                      (GtkMozEmbed       *mozilla_embed,
							    const char        *encoding);
guint    mozilla_charset_get_num_encodings                 (const GtkMozEmbed *mozilla_embed);
char *   mozilla_charset_get_nth_encoding                  (const GtkMozEmbed *mozilla_embed,
							    guint              n);
char *   mozilla_charset_get_nth_encoding_title            (const GtkMozEmbed *mozilla_embed,
							    guint              n);
char *   mozilla_charset_get_nth_translated_encoding_title (const GtkMozEmbed *mozilla_embed,
							    guint              n);
char *   mozilla_charset_find_encoding_group               (const GtkMozEmbed *mozilla_embed,
							    const char        *encoding);
char *   mozilla_charset_encoding_group_get_translated     (const GtkMozEmbed *mozilla_embed,
							    const char        *encoding);
int      mozilla_charset_get_encoding_group_index          (const GtkMozEmbed *mozilla_embed,
							    const char        *encoding_group);
char *   mozilla_get_document_title                        (const GtkMozEmbed *mozilla_embed);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NAUTILUS_MOZILLA_EMBED_EXTENSIONS_H */
