/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999 Red Hat, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */
/* ntl-main.c: Implementation of the routines that drive program lifecycle and main window creation/destruction. */

#include "config.h"
#include "nautilus.h"

int main(int argc, char *argv[])
{
  poptContext ctx;
  CORBA_Environment ev;
  CORBA_ORB orb;
  struct poptOption options[] = {
    { NULL, '\0', 0, NULL, 0, NULL, NULL }
  };
  const char **args;

  /* FIXME: This should also include G_LOG_LEVEL_WARNING, but I had to take it
   * out temporarily so we could continue to work on other parts of the software
   * until the only-one-icon-shows-up problem is fixed
   */
  if (getenv("NAUTILUS_DEBUG"))
    g_log_set_always_fatal(G_LOG_FATAL_MASK | G_LOG_LEVEL_CRITICAL);

  orb = gnome_CORBA_init_with_popt_table("nautilus", VERSION, &argc, argv, options, 0, &ctx, GNORBA_INIT_SERVER_FUNC, &ev);
  bonobo_init(orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);
  g_thread_init(NULL);
  gnome_vfs_init();

  args = poptGetArgs(ctx);
  nautilus_app_init(args?args[0]:NULL);

  bonobo_main();
  return 0;
}
