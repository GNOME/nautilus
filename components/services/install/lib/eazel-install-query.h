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
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 */

#ifndef EAZEL_INSTALL_QUERY_H
#define EAZEL_INSTALL_QUERY_H

#include "eazel-install-types.h"
#include "eazel-install-public.h"

typedef enum {
	EI_SIMPLE_QUERY_OWNS,
	EI_SIMPLE_QUERY_PROVIDES,
	EI_SIMPLE_QUERY_REQUIRES,
	EI_SIMPLE_QUERY_MATCHES
} EazelInstallSimpleQueryEnum;

/* Performs a simple query for "input", using
   the enum flag.
   
   neglists is the number of GList* provided to the function. These
   lists should contain pacakage that should _not_ be returned in the
   result. So normally you will just use it like :
   
   GList *matches = eazel_install_simple_query (service, "somepackage", 
                                                EI_SIMPLE_QUERY_MATCHES, 0, NULL);
*/
   
GList* eazel_install_simple_query (EazelInstall *service, 
				   const char *input, 
				   EazelInstallSimpleQueryEnum flag, 
				   int neglists, ...);

#endif /* EAZEL_INSTALL_QUERY_H */
