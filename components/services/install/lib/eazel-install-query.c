/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 * Copyright (C) 1998-1999 James Henstridge
 * Copyright (C) 1998 Red Hat Software, Inc.
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
 * Authors: Eskil Heyn Olsen  <eskil@eazel.com>
 */

#include <eazel-install-query.h>
#include <ctype.h>

/*****************************************************************************/
/* Query mechanisms                                                          */
/*****************************************************************************/

/*

  query syntax :
  " '<package>'.[<attr><op>'<arg>']*

  attr and op pairs :
  -------------------
  version = < > <= >=
  arch    =
  
  query examples:
  "'gnome-core'" check for package gnome-core
  "'nautilus'.version>='0.1'.arch=i386" Check for nautilus, i386 binary version >= 0.1
  "'popt'.version='1.5'" check for popt version 1.5 (no more, no less)

*/  

typedef enum {
	EI_Q_VERSION,
	EI_Q_ARCH
} AttributeEnum;

typedef enum {
	EI_OP_EQ = 0x01,
	EI_OP_LT = 0x02,
	EI_OP_GT = 0x04,
	EI_OP_NEG = 0x08,
} AttributeOpEnum;

typedef struct {
	AttributeEnum attrib;
	AttributeOpEnum op;
	char *arg;
} AttributeOpArg;

GList*
eazel_install_query_package_system (EazelInstall *service,
				    const char *query, 
				    int flags) 
{
	g_message ("eazel_install_query_package_system (...,%s,...)", query);

	/* FIXME bugzilla.eazel.com 1445
	   write and use a yacc parser for the queries here.
	   The lexer is in eazel-install-query-lex.l
	*/

	return NULL;
}

GList* 
eazel_install_simple_query (EazelInstall *service, char *input, 
			    SimpleQueryEnum flag, 
			    int neglists, 
			    GList *neglist,...)
{
	dbiIndexSet matches;
	rpmdb db;
	int rc;
	int i;

	/* db = service->private->packsys.rpm.db; */
	
	switch (flag) {
	case EI_SIMPLE_QUERY_PROVIDES:
	case EI_SIMPLE_QUERY_REQUIRES:
	case EI_SIMPLE_QUERY_MATCHES:
	}
}
