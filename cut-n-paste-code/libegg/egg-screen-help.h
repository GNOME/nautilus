/* egg-screen-help.h
 *
 * Copyright (C) 2002  Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __EGG_SCREEN_HELP_H__
#define __EGG_SCREEN_HELP_H__

#include <glib.h>
#include <gdk/gdk.h>
#include <libgnome/gnome-program.h>

G_BEGIN_DECLS

/* Destined for libgnomeui.
 */
gboolean egg_help_display_on_screen             (const char    *file_name,
						 const char    *link_id,
						 GdkScreen     *screen,
						 GError       **error);
gboolean egg_help_display_with_doc_id_on_screen (GnomeProgram  *program,
						 const char    *doc_id,
						 const char    *file_name,
						 const char    *link_id,
						 GdkScreen     *screen,
						 GError       **error);
gboolean egg_help_display_desktop_on_screen     (GnomeProgram  *program,
						 const char    *doc_id,
						 const char    *file_name,
						 const char    *link_id,
						 GdkScreen     *screen,
						 GError       **error);
gboolean egg_help_display_uri_on_screen         (const char    *help_uri,
						 GdkScreen     *screen,
						 GError       **error);

G_END_DECLS

#endif /* __EGG_SCREEN_HELP_H__ */
