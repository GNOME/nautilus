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
	        EAZEL_SOFTCAT_FILL_FLAG_EVERYTHING = 0x0,
		EAZEL_SOFTCAT_FILL_FLAG_NO_TEXT = 0x1,
		EAZEL_SOFTCAT_FILL_FLAG_NO_FILES = 0x2,
		EAZEL_SOFTCAT_FILL_FLAG_NO_DEPENDENCIES = 0x4
} EazelSoftCatFillFlags;

typedef enum {
	EAZEL_SOFTCAT_NO_ERROR = 0,
	EAZEL_SOFTCAT_INCORRECT_PARAMETERS,
	EAZEL_SOFTCAT_NO_SERVER_CONNECTION,
	EAZEL_SOFTCAT_SERVER_MALFUNCTION
} EazelSoftCatError;

typedef enum {
	EAZEL_SOFTCAT_SENSE_EQ = 0x1,
	EAZEL_SOFTCAT_SENSE_GT = 0x2,
	EAZEL_SOFTCAT_SENSE_LT = 0x4
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

/* Sets the SoftCat server, in <name>[:port] format */
void eazel_softcat_set_server (EazelSoftCat *,
			       const char *server);

/* Set the retry parameters */
void eazel_softcat_set_retry (EazelSoftCat*,
			      unsigned int number_of_retries,
			      unsigned int delay_between_attempts);

#endif /* EAZEL_SOFTCAT_PUBLIC_H */

