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
 * Authors: J Shane Culpepper <pepper@eazel.com>
 */

/* eazel-install - services command line install/update/uninstall
 * component.  This program will parse the eazel-services-config.xml
 * file and install a services generated package-list.xml.
 */

#ifndef __EAZEL_SERVICES__RPM_GLUE_H__
#define __EAZEL_SERVICES__RPM_GLUE_H__

#include "eazel-install-types.h"
#include "helixcode-install-utils.h"
#include "eazel-install-xml-package-list.h"
#include <rpm/rpmlib.h>
#include <rpm/rpmurl.h>

gboolean install_new_packages (InstallOptions* iopts);
gboolean uninstall_packages (InstallOptions* iopts);


#endif /* __EAZEL_SERVICES_RPM_GLUE_H__ */
