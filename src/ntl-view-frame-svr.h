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
/* ntl-view-frame-svr.h: CORBA server implementation of the object
   representing a data view frame. */


#ifndef __NAUTILUS_VIEW_FRAME_SVR_H__
#define __NAUTILUS_VIEW_FRAME_SVR_H__

#include "nautilus.h"

POA_Nautilus_ViewFrame__vepv impl_Nautilus_ViewFrame_vepv;
GnomeObject *impl_Nautilus_ViewFrame__create(NautilusView *view, CORBA_Environment * ev);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __NAUTILUS_VIEW_H__ */

