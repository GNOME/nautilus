/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nautilus-application-smclient: a little module for session handling.
 *
 * Copyright (C) 2000 Red Hat, Inc.
 * Copyright (C) 2010 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __NAUTILUS_APPLICATION_SMCLIENT_H__
#define __NAUTILUS_APPLICATION_SMCLIENT_H__

#include "nautilus-application.h"

void nautilus_application_smclient_startup (NautilusApplication *self);
void nautilus_application_smclient_load (NautilusApplication *self,
					 gboolean *no_default_window);

#endif /* __NAUTILUS_APPLICATION_SMCLIENT_H__ */
