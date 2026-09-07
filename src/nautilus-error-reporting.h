/*
 * SPDX-FileCopyrightText: 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 */

#pragma once

#include "nautilus-types.h"

#include "nautilus-file.h"

#include <gtk/gtk.h>

#define FAT_FORBIDDEN_CHARACTERS                ":|<>*?\\\"/"

void nautilus_report_error_loading_directory	 (NautilusFile   *file,
						  GError         *error,
						  GtkWidget	 *parent);
void nautilus_report_error_renaming_file         (NautilusFile *file,
						  const char *new_name,
						  GError *error,
						  GtkWidget *parent);
void nautilus_report_error_setting_permissions (NautilusFile   *file,
						GError         *error,
						GtkWidget    *parent);
void nautilus_report_error_setting_owner       (NautilusFile   *file,
						GError         *error,  
						GtkWidget    *parent);
void nautilus_report_error_setting_group       (NautilusFile   *file,
						GError         *error,
						GtkWidget    *parent);
