/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
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
 * Authors: Ramiro Estrugo <ramiro@eazel.com>
 */

#ifndef NAUTILUS_AUTHENTICATE_H
#define NAUTILUS_AUTHENTICATE_H

#include <stdlib.h>
#include <glib.h>

gboolean nautilus_authenticate_fork         (const char *command,
					     gint       *pid_out);
gboolean nautilus_authenticate_authenticate (const char *username,
					     const char *password);

#endif /* NAUTILUS_AUTHENTICATE_H */

