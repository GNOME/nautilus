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

/* nautilus-profiler.h: Nautilus profiler hooks and reporting. */

#ifndef NAUTILUS_PROFILER_H
#define NAUTILUS_PROFILER_H

#include <libnautilus-private/nautilus-bonobo-extensions.h>

void nautilus_profiler_bonobo_ui_reset_callback  (BonoboUIComponent *component,
						  gpointer           user_data,
						  const char        *verb);
void nautilus_profiler_bonobo_ui_start_callback  (BonoboUIComponent *component,
						  gpointer           user_data,
						  const char        *verb);
void nautilus_profiler_bonobo_ui_stop_callback   (BonoboUIComponent *component,
						  gpointer           user_data,
						  const char        *verb);
void nautilus_profiler_bonobo_ui_report_callback (BonoboUIComponent *component,
						  gpointer           user_data,
						  const char        *verb);

#endif /* NAUTILUS_PROFILER_H */
