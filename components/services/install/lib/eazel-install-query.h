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

GList* eazel_install_query_package_system (EazelInstall *service, const char *query, int flags);

typedef enum {
	EI_SIMPLE_QUERY_PROVIDES,
	EI_SIMPLE_QUERY_REQUIRES,
	EI_SIMPLE_QUERY_MATCHES
} SimpleQueryEnum;

GList* eazel_install_simple_query (EazelInstall *service, char *input, SimpleQueryEnum flag, int neglists, GList *neglist,...);

#endif /* EAZEL_INSTALL_QUERY_H */
