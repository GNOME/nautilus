/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000, 2001  Eazel, Inc
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
 *          Robey Pointer <robey@eazel.com>
 */

#ifndef _CALLBACKS_H_
#define _CALLBACKS_H_

void nautilus_service_install_conflict_check (EazelInstallCallback *cb, const PackageData *pack,
                                              NautilusServiceInstallView *view);
void nautilus_service_install_dependency_check (EazelInstallCallback *cb, const PackageData *package,
                                                const PackageData *needs, NautilusServiceInstallView *view);
gboolean nautilus_service_install_preflight_check (EazelInstallCallback *cb, 
                                                   EazelInstallCallbackOperation op,
                                                   const GList *packages,
                                                   int total_bytes, 
                                                   int total_packages,
                                                   NautilusServiceInstallView *view);
gboolean nautilus_service_install_save_transaction (EazelInstallCallback *cb, 
						    EazelInstallCallbackOperation op,
						    const GList *packages,
						    NautilusServiceInstallView *view);
void nautilus_service_install_download_progress (EazelInstallCallback *cb, const PackageData *pack,
                                                 int amount, int total,
                                                 NautilusServiceInstallView *view);
void nautilus_service_install_download_failed (EazelInstallCallback *cb, const PackageData *pack,
                                               NautilusServiceInstallView *view);
void nautilus_service_install_progress (EazelInstallCallback *cb, const PackageData *pack,
                                        int current_package, int total_packages,
                                        int package_progress, int package_total,
                                        int total_progress, int total_total,
                                        NautilusServiceInstallView *view);
void nautilus_service_install_failed (EazelInstallCallback *cb, 
				      PackageData *package,
                                      NautilusServiceInstallView *view);
void nautilus_service_install_done (EazelInstallCallback *cb, 
				    gboolean success, 
				    NautilusServiceInstallView *view);


#endif	/* _CALLBACKS_H_ */
