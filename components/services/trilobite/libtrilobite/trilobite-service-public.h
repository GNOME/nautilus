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

/* trilobite-service.h: Interface for objects representing
   the toplevel type for services ("trilobites").
 */

#ifndef TRILOBITE_SERVICE_H
#define TRILOBITE_SERVICE_H

#include <libgnome/gnome-defs.h>
#include <bonobo/bonobo-object.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define TRILOBITE_TYPE_SERVICE                          (trilobite_service_get_type ())
#define TRILOBITE_SERVICE(obj)                          (GTK_CHECK_CAST ((obj), TRILOBITE_TYPE_SERVICE, TrilobiteService))
#define TRILOBITE_SERVICE_CLASS(klass)                  (GTK_CHECK_CLASS_CAST ((klass), TRILOBITE_TYPE_SERVICE, TrilobiteServiceClass))
#define TRILOBITE_IS_SERVICE(obj)                       (GTK_CHECK_TYPE ((obj), TRILOBITE_TYPE_SERVICE))
#define TRILOBITE_IS_SERVICE_CLASS(klass)               (GTK_CHECK_CLASS_TYPE ((klass), TRILOBITE_TYPE_SERVICE))
	
typedef struct _TrilobiteService TrilobiteService;
typedef struct _TrilobiteServiceClass TrilobiteServiceClass;

struct _TrilobiteServiceClass 
{
	BonoboObjectClass parent_class;

	char* (*get_name)         (TrilobiteService *trilobite);
	char* (*get_version)      (TrilobiteService *trilobite);
	char* (*get_vendor_name)  (TrilobiteService *trilobite);
	char* (*get_vendor_url)   (TrilobiteService *trilobite);
	char* (*get_url)          (TrilobiteService *trilobite);
	char* (*get_icon)         (TrilobiteService *trilobite);

	void (*set_name)         (TrilobiteService *trilobite, const char *value);
	void (*set_version)      (TrilobiteService *trilobite, const char *value);
	void (*set_vendor_name)  (TrilobiteService *trilobite, const char *value);
	void (*set_vendor_url)   (TrilobiteService *trilobite, const char *value);
	void (*set_url)          (TrilobiteService *trilobite, const char *value);
	void (*set_icon)         (TrilobiteService *trilobite, const char *value);

	gpointer servant_init;
	gpointer servant_fini;
	gpointer servant_vepv;
};

typedef struct _TrilobiteServicePrivate TrilobiteServicePrivate;

struct _TrilobiteService
{
	BonoboObject                 parent;
	TrilobiteServicePrivate     *private;
};

GtkType                       trilobite_service_get_type   (void);
gboolean                      trilobite_service_construct  (TrilobiteService *trilobite, Trilobite_Service corba_trilobite);
TrilobiteService*             trilobite_service_new        (void);
POA_Trilobite_Service__epv*   trilobite_service_get_epv    (void);
void                          trilobite_service_destroy    (GtkObject *object);

/* This should be called from the service factory.
   It adds the interface and any data that you might need access to */
void               trilobite_service_add_interface (TrilobiteService *trilobite, 
						    BonoboObject *service);

char*              trilobite_service_get_name            (TrilobiteService *trilobite);
char*              trilobite_service_get_version         (TrilobiteService *trilobite);
char*              trilobite_service_get_vendor_name     (TrilobiteService *trilobite);
char*              trilobite_service_get_vendor_url      (TrilobiteService *trilobite);
char*              trilobite_service_get_url             (TrilobiteService *trilobite);
char*              trilobite_service_get_icon            (TrilobiteService *trilobite);

void               trilobite_service_set_name            (TrilobiteService *trilobite, const char *value);
void               trilobite_service_set_version         (TrilobiteService *trilobite, const char *value);
void               trilobite_service_set_vendor_name     (TrilobiteService *trilobite, const char *value);
void               trilobite_service_set_vendor_url      (TrilobiteService *trilobite, const char *value);
void               trilobite_service_set_url             (TrilobiteService *trilobite, const char *value);
void               trilobite_service_set_icon            (TrilobiteService *trilobite, const char *value);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TRILOBITE_SERVICE_H */
