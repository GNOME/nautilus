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

#ifndef _FORMS_H_
#define _FORMS_H_

void install_message_destroy (InstallMessage *im);
InstallMessage *install_message_new (NautilusServiceInstallView *view, const char *package_name);
void generate_install_form (NautilusServiceInstallView *view);
void show_overall_feedback (NautilusServiceInstallView *view, char *progress_message);
void update_package_info_display (NautilusServiceInstallView *view, const PackageData *pack, const char *format);
void current_progress_bar_complete (NautilusServiceInstallView *view, const char *text);
void make_query_box (NautilusServiceInstallView *view, EazelInstallCallbackOperation op, GList *package_list);

#endif	/* _FORMS_H_ */
