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

#ifndef EAZEL_INSTALLER_CALLBACKS_H
#define EAZEL_INSTALLER_CALLBACKS_H

#include "gnome-druid.h"
#include "installer.h"

void
druid_cancel                           (GnomeDruid      *gnomedruid,
					EazelInstaller  *installer);
void
druid_delete (GtkWidget *widget, GdkEvent *event, EazelInstaller *installer);

void
druid_finish                           (GnomeDruidPage  *gnomedruidpage,
                                        gpointer         arg1,
					EazelInstaller  *installer);

gboolean
begin_install                          (EazelInstaller  *installer);

void
prep_install                           (GnomeDruidPage  *gnomedruidpage,
					GtkWidget *druid,
					EazelInstaller  *installer);
void
prep_finish                           (GnomeDruidPage  *gnomedruidpage,
				       GtkWidget *druid,
				       EazelInstaller  *installer);
void
prep_retry                            (GnomeDruidPage  *gnomedruidpage,
				       GtkWidget *druid,
				       EazelInstaller  *installer);

#endif /* EAZEL_INSTALLER_CALLBACKS_H */

