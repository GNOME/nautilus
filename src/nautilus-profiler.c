/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ramiro Estrugo <ramiro@eazel.com>
 */

/* nautilus-profiler.c: Nautilus profiler hooks and reporting.
 */

#include <config.h>
#include <gtk/gtkdialog.h>

#include "nautilus-profiler.h"

/* These are defined in eazel-tools/profiler/profiler.C */
extern void profile_on (void);
extern void profile_off (void);
extern void profile_reset (void);
extern void profile_dump (void);

void
nautilus_profiler_bonobo_ui_reset_callback (BonoboUIHandler *ui_handler, 
					    gpointer user_data,
					    const char *path)
{
	profile_reset ();
}

void
nautilus_profiler_bonobo_ui_start_callback (BonoboUIHandler *ui_handler, 
					    gpointer user_data,
					    const char *path)
{
	profile_on ();
}
void
nautilus_profiler_bonobo_ui_stop_callback (BonoboUIHandler *ui_handler, 
					   gpointer user_data,
					   const char *path)
{
	profile_off ();
}
void
nautilus_profiler_bonobo_ui_report_callback (BonoboUIHandler *ui_handler, 
					     gpointer user_data,
					     const char *path)
{
	profile_dump ();
}
