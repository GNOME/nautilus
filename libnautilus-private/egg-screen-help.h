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

/* Destined for libgnome.
 */
gboolean egg_help_display_uri_with_env         (const char    *help_uri,
						char         **envp,
						GError       **error);
gboolean egg_help_display_with_doc_id_with_env (GnomeProgram  *program,
						const char    *doc_id,
						const char    *file_name,
						const char    *link_id,
						char         **envp,
						GError       **error);
gboolean egg_help_display_desktop_with_env     (GnomeProgram  *program,
						const char    *doc_id,
						const char    *file_name,
						const char    *link_id,
						char         **envp,
						GError       **error);

/* Destined for libgnomeui.
 */
gboolean egg_screen_help_display             (GdkScreen     *screen,
					      const char    *file_name,
					      const char    *link_id,
					      GError       **error);
gboolean egg_screen_help_display_with_doc_id (GdkScreen     *screen,
					      GnomeProgram  *program,
					      const char    *doc_id,
					      const char    *file_name,
					      const char    *link_id,
					      GError       **error);
gboolean egg_screen_help_display_desktop     (GdkScreen     *screen,
					      GnomeProgram  *program,
					      const char    *doc_id,
					      const char    *file_name,
					      const char    *link_id,
					      GError       **error);
gboolean egg_screen_help_display_uri         (GdkScreen      *screen,
					      const char    *help_uri,
					      GError       **error);

 
G_END_DECLS

#endif /* __EGG_SCREEN_HELP_H__ */
