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
 * Authors: Robey Pointer <robey@eazel.com>
 *          Eskil Heyn Olsen <eskil@eazel.com>
 *
 */

#ifndef EAZEL_SOFTCAT_PUBLIC_H
#define EAZEL_SOFTCAT_PUBLIC_H

#include "eazel-package-system-types.h"
#include "eazel-softcat-private.h"

#define TYPE_EAZEL_SOFTCAT           (eazel_softcat_get_type ())
#define EAZEL_SOFTCAT(obj)           (GTK_CHECK_CAST ((obj), TYPE_EAZEL_SOFTCAT, EazelSoftCat))
#define EAZEL_SOFTCAT_CLASS(klass)   (GTK_CHECK_CLASS_CAST ((klass), TYPE_EAZEL_SOFTCAT, EazelSoftCatClass))
#define EAZEL_IS_SOFTCAT(obj)        (GTK_CHECK_TYPE ((obj), TYPE_EAZEL_SOFTCAT))
#define EAZEL_IS_SOFTCAT_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((klass), TYPE_EAZEL_SOFTCAT))

typedef struct _EazelSoftCat EazelSoftCat;
typedef struct _EazelSoftCatClass EazelSoftCatClass;

typedef enum {
	EAZEL_SOFTCAT_SUCCESS = 0,
	EAZEL_SOFTCAT_ERROR_BAD_MOJO,
	EAZEL_SOFTCAT_ERROR_SERVER_UNREACHABLE,
	EAZEL_SOFTCAT_ERROR_MULTIPLE_RESPONSES,
	EAZEL_SOFTCAT_ERROR_NO_SUCH_PACKAGE
} EazelSoftCatError;

struct _EazelSoftCatClass
{
	GtkObjectClass parent_class;
};

typedef struct _EazelSoftCatPrivate EazelSoftCatPrivate;

struct _EazelSoftCat
{
	GtkObject parent;
	EazelSoftCatPrivate *private;
};

EazelSoftCat   *eazel_softcat_new (void);
GtkType         eazel_softcat_get_type (void);
void		eazel_softcat_unref (GtkObject *object);

/* set and get fields */
void eazel_softcat_set_server (EazelSoftCat *softcat, const char *server);
void eazel_softcat_set_server_host (EazelSoftCat *softcat, const char *server);
void eazel_softcat_set_server_port (EazelSoftCat *softcat, int port);
const char *eazel_softcat_get_server (EazelSoftCat *softcat);
const char *eazel_softcat_get_server_host (EazelSoftCat *softcat);
int eazel_softcat_get_server_port (EazelSoftCat *softcat);
void eazel_softcat_set_cgi_path (EazelSoftCat *softcat, const char *cgi_path);
const char *eazel_softcat_get_cgi_path (const EazelSoftCat *softcat);
void eazel_softcat_set_authn (EazelSoftCat *softcat, gboolean use_authn, const char *username);
void eazel_softcat_set_authn_flag (EazelSoftCat *softcat, gboolean use_authn);
void eazel_softcat_set_username (EazelSoftCat *softcat, const char *username);
gboolean eazel_softcat_get_authn (const EazelSoftCat *softcat, const char **username);
void eazel_softcat_set_retry (EazelSoftCat *softcat, unsigned int retries, unsigned int delay_us);

EazelSoftCatSense eazel_softcat_convert_sense_flags (int flags);
char *eazel_softcat_sense_flags_to_string (EazelSoftCatSense flags);
EazelSoftCatSense eazel_softcat_string_to_sense_flags (const char *str);

const char *eazel_softcat_error_string (EazelSoftCatError err);

/* Query softcat about a package, and return a list of matching packages
 * (because there may be more than one if the package refers to a suite).
 */
EazelSoftCatError eazel_softcat_query (EazelSoftCat *softcat,
				       PackageData *package,
				       int sense_flags,
				       int fill_flags,
				       GList **result);

/* Given a partially filled packagedata object, 
   check softcat, and fill it with the desired info */
EazelSoftCatError  eazel_softcat_get_info (EazelSoftCat *softcat,
					   PackageData *partial,
					   int sense_flags,
					   int fill_flags);

/* Check if there's a newer version in SoftCat.
 * Returns TRUE and fills in 'newpack' if there is, returns FALSE otherwise.
 */
gboolean eazel_softcat_available_update (EazelSoftCat *softcat,
					 PackageData *oldpack,
					 PackageData **newpack,
					 int fill_flags);

#endif /* EAZEL_SOFTCAT_PUBLIC_H */
