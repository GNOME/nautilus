/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000 Eazel, Inc.
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
 *  Authors: Elliot Lee <sopwith@redhat.com>,
 *           Darin Adler <darin@eazel.com>,
 *           John Sullivan <sullivan@eazel.com>
 *
 */

/* ntl-main.c: Implementation of the routines that drive program lifecycle and main window creation/destruction. */

#include "config.h"
#include "nautilus.h"
#include "nautilus-self-check-functions.h"
#include <libnautilus/nautilus-lib-self-check-functions.h>
#include <libnautilus/nautilus-self-checks.h>

int main(int argc, char *argv[])
{
  poptContext ctx;
  CORBA_Environment ev;
  CORBA_ORB orb;
#if !defined (NAUTILUS_OMIT_SELF_CHECK)
  gboolean check = FALSE;
#endif
  const char **args;

  struct poptOption options[] = {
#if !defined (NAUTILUS_OMIT_SELF_CHECK)
    { "check", '\0', POPT_ARG_NONE, &check, 0, N_("Perform high-speed self-check tests."), NULL },
    POPT_AUTOHELP
#endif
    { NULL, '\0', 0, NULL, 0, NULL, NULL }
  };

  /* FIXME: This should also include G_LOG_LEVEL_WARNING, but I had to take it
   * out temporarily so we could continue to work on other parts of the software
   * until the only-one-icon-shows-up problem is fixed
   */
  if (getenv("NAUTILUS_DEBUG"))
    g_log_set_always_fatal (G_LOG_FATAL_MASK | G_LOG_LEVEL_CRITICAL);

  CORBA_exception_init(&ev);
  orb = gnome_CORBA_init_with_popt_table ("nautilus", VERSION, &argc, argv, options, 0, &ctx, GNORBA_INIT_SERVER_FUNC, &ev);
  bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);
  g_thread_init (NULL);
  gnome_vfs_init ();

  args = poptGetArgs (ctx);

#if !defined (NAUTILUS_OMIT_SELF_CHECK)
  if (check) {
    /* Run the checks for nautilus and libnautilus. */
    nautilus_run_self_checks ();
    nautilus_run_lib_self_checks ();
    if (nautilus_self_checks_failed ())
      return 1;
  } else
#endif
  {
    nautilus_app_init (args ? args[0] : NULL);

    bonobo_main();

    /* One last chance for cleanup before program finishes. */
    nautilus_app_exiting();
  }

  return 0;
}
