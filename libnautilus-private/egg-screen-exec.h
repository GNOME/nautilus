/* egg-screen-exec.h
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

#ifndef __EGG_SCREEN_EXEC_H__
#define __EGG_SCREEN_EXEC_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

char      *egg_screen_exec_display_string        (GdkScreen    *screen);
char     **egg_screen_exec_environment           (GdkScreen    *screen);

int        egg_screen_execute_async              (GdkScreen    *screen,
                                                  const char   *dir,
                                                  int           argc,
                                                  char * const  argv []);
int        egg_screen_execute_shell              (GdkScreen    *screen,
                                                  const char   *dir,
                                                  const char   *command);
gboolean   egg_screen_execute_command_line_async (GdkScreen    *screen,
                                                  const char   *command,
                                                  GError      **error);

G_END_DECLS

#endif /* __EGG_SCREEN_EXEC_H__ */
