/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 *
 */

#ifndef TRILOBITE_EAZEL_TIME_SERVICE_PUBLIC_H
#define TRILOBITE_EAZEL_TIME_SERVICE_PUBLIC_H 

#include <libgnome/gnome-defs.h>
#include "trilobite-eazel-time-service.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define TRILOBITE_TYPE_EAZEL_TIME_SERVICE           (trilobite_eazel_time_service_get_type ())
#define TRILOBITE_EAZEL_TIME_SERVICE(obj)           (GTK_CHECK_CAST ((obj), TRILOBITE_TYPE_EAZEL_TIME_SERVICE, TrilobiteEazelTimeService))
#define TRILOBITE_EAZEL_TIME_SERVICE_CLASS(klass)   (GTK_CHECK_CLASS_CAST ((klass), TRILOBITE_TYPE_EAZEL_TIME_SERVICE, TrilobiteEazelTimeServiceClass))
#define TRILOBITE_IS_EAZEL_TIME_SERVICE(obj)        (GTK_CHECK_TYPE ((obj), TRILOBITE_TYPE_EAZEL_TIME_SERVICE))
#define TRILOBITE_IS_EAZEL_TIME_SERVICE_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((klass), TRILOBITE_TYPE_EAZEL_TIME_SERVICE))

typedef struct _TrilobiteEazelTimeService TrilobiteEazelTimeService;
typedef struct _TrilobiteEazelTimeServiceClass TrilobiteEazelTimeServiceClass;

struct _TrilobiteEazelTimeServiceClass 
{
	BonoboObjectClass parent_class;

	gpointer servant_vepv;
};

typedef struct _TrilobiteEazelTimeServicePrivate TrilobiteEazelTimeServicePrivate;

struct _TrilobiteEazelTimeService
{
	BonoboObject parent;
	TrilobiteEazelTimeServicePrivate *private;
};

GtkType                        trilobite_eazel_time_service_get_type   (void);
TrilobiteEazelTimeService*     trilobite_eazel_time_service_new        (void);
POA_Trilobite_Eazel_Time__epv* trilobite_eazel_time_service_get_epv    (void);
void                           trilobite_eazel_time_service_destroy    (GtkObject *object);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TRILOBITE_EAZEL_TIME_SERVICE_PUBLIC_H */
