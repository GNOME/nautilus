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

#ifndef EAZEL_SOFTCAT_PUBLIC_H
#define EAZEL_SOFTCAT_PUBLIC_H

#include "eazel-install-types.h"
#include "eazel-softcat-private.h"

#define TYPE_EAZEL_SOFTCAT           (eazel_softcat_get_type ())
#define EAZEL_SOFTCAT(obj)           (GTK_CHECK_CAST ((obj), TYPE_EAZEL_SOFTCAT, EazelSoftCat))
#define EAZEL_SOFTCAT_CLASS(klass)   (GTK_CHECK_CLASS_CAST ((klass), TYPE_EAZEL_SOFTCAT, EazelSoftCatClass))
#define IS_EAZEL_SOFTCAT(obj)        (GTK_CHECK_TYPE ((obj), TYPE_EAZEL_SOFTCAT))
#define IS_EAZEL_SOFTCAT_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((klass), TYPE_EAZEL_SOFTCAT))

typedef struct _EazelSoftCat EazelSoftCat;
typedef struct _EazelSoftCatClass EazelSoftCatClass;

typedef enum {
	EAZEL_SOFTCAT_SUCCESS = 0,
	EAZEL_SOFTCAT_ERROR_BAD_MOJO,
	EAZEL_SOFTCAT_ERROR_SERVER_UNREACHABLE,
	EAZEL_SOFTCAT_ERROR_NO_SUCH_PACKAGE
} EazelSoftCatError;

typedef enum {
	EAZEL_SOFTCAT_SENSE_EQ = 0x1,
	EAZEL_SOFTCAT_SENSE_GT = 0x2,
	EAZEL_SOFTCAT_SENSE_LT = 0x4,
	EAZEL_SOFTCAT_SENSE_GE = (EAZEL_SOFTCAT_SENSE_GT | EAZEL_SOFTCAT_SENSE_EQ)
} EazelSoftCatSense;

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

EazelSoftCat  *eazel_softcat_new (void);
GtkType        eazel_softcat_get_type   (void);

/* set and get fields */
void eazel_softcat_set_server (EazelSoftCat *softcat, const char *server);
const char *eazel_softcat_get_server (EazelSoftCat *softcat);
void eazel_softcat_set_cgi_path (EazelSoftCat *softcat, const char *cgi_path);
const char *eazel_softcat_get_cgi_path (const EazelSoftCat *softcat);
void eazel_softcat_set_authn (EazelSoftCat *softcat, gboolean use_authn, const char *username);
gboolean eazel_softcat_get_authn (const EazelSoftCat *softcat, const char **username);
void eazel_softcat_set_retry (EazelSoftCat *softcat, unsigned int retries, unsigned int delay_us);

const char *eazel_softcat_error_string (EazelSoftCatError err);

/* Check if theres a newer version in SoftCat.
   Returns TRUE and fill in new if there is, returns
   FALSE otherwise. new is filled by calling get_info */
EazelSoftCatError  eazel_softcat_available_update (EazelSoftCat*, 
						   PackageData *old, 
						   PackageData **new,
						   int fill_flags);

/* Given a partially filled packagedata object, 
   check softcat, and fill it with the desired info */
EazelSoftCatError  eazel_softcat_get_info (EazelSoftCat*,
					   PackageData *partial,
					   int sense_flags,
					   int fill_flags);

#endif /* EAZEL_SOFTCAT_PUBLIC_H */

