/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
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
 *  Author: Gene Z. Ragan <gzr@eazel.com>
 *
 */

#ifndef NAUTILUS_HISTORY_FRAME_H
#define NAUTILUS_HISTORY_FRAME_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "nautilus-view-frame.h"

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-main.h>
#include <libnautilus/nautilus-view-component.h>



BonoboObject *impl_Nautilus_HistoryFrame__create	(NautilusViewFrame 	*view,
							 CORBA_Environment 	*ev);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NAUTILUS_HISTORY_FRAME_H */
