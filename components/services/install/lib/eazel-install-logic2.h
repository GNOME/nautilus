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

#ifndef EAZEL_INSTALL_LOGIC2_H
#define EAZEL_INSTALL_LOGIC2_H

#include "eazel-package-system-types.h"
#include "eazel-install-protocols.h"
#include "eazel-install-public.h"
#include <eazel-package-system.h>

/* FIXME: these should not be exported once eazel-install-logic is being
   phased out */
gboolean check_md5_on_files (EazelInstall *service, GList *packages);
EazelInstallStatus eazel_install_check_existing_packages (EazelInstall *service, 
							  PackageData *pack);
unsigned long eazel_install_get_total_size_of_packages (EazelInstall *service,
							const GList *packages);

EazelInstallOperationStatus install_packages (EazelInstall *service, GList *categories);
EazelInstallOperationStatus uninstall_packages (EazelInstall *service, GList *categories);
EazelInstallOperationStatus revert_transaction (EazelInstall *service, GList *packages);

#endif /* EAZEL_INSTALL_LOGIC_H */
