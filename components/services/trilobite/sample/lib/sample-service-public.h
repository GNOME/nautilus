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

#ifndef SAMPLE_SERVICE_PUBLIC_H
#define SAMPLE_SERVICE_PUBLIC_H 

#include <libgnome/gnome-defs.h>
#include "sample-service.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define SAMPLE_TYPE_SERVICE                          (sample_service_get_type ())
#define SAMPLE_SERVICE(obj)                          (GTK_CHECK_CAST ((obj), SAMPLE_TYPE_SERVICE, SampleService))
#define SAMPLE_SERVICE_CLASS(klass)                  (GTK_CHECK_CLASS_CAST ((klass), SAMPLE_TYPE_SERVICE, SampleServiceClass))
#define SAMPLE_IS_SERVICE(obj)                       (GTK_CHECK_TYPE ((obj), SAMPLE_TYPE_SERVICE))
#define SAMPLE_IS_SERVICE_CLASS(klass)               (GTK_CHECK_CLASS_TYPE ((klass), SAMPLE_TYPE_SERVICE))
	
typedef struct _SampleService SampleService;
typedef struct _SampleServiceClass SampleServiceClass;

struct _SampleServiceClass 
{
	BonoboObjectClass parent_class;

	void (*remember) (SampleService *service, const char *something);
	void (*say_it)   (SampleService *service);

	gpointer servant_vepv;
};

typedef struct _SampleServicePrivate SampleServicePrivate;

struct _SampleService
{
	BonoboObject parent;
	char *my_string;
};

GtkType                       sample_service_get_type   (void);
SampleService*                sample_service_new        (void);
POA_Trilobite_Eazel_Sample__epv* sample_service_get_epv    (void);
void                          sample_service_destroy    (GtkObject *object);

void              sample_service_remember             (SampleService *sample, const char *something);
void              sample_service_say_it               (SampleService *sample);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SAMPLE_SERVICE_PUBLIC_H */
