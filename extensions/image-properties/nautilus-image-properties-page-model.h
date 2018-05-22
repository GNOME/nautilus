/* Copyright (C) 2004 Red Hat, Inc
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#pragma once

#include <gtk/gtk.h>

#include <nautilus-extension.h>

#define NAUTILUS_TYPE_IMAGE_PROPERTIES_PAGE_MODEL (nautilus_image_properties_page_model_get_type ())

G_DECLARE_FINAL_TYPE (NautilusImagePropertiesPageModel,
                      nautilus_image_properties_page_model,
                      NAUTILUS, IMAGE_PROPERTIES_PAGE_MODEL,
                      NautilusPropertyPageModel)

void                          nautilus_image_properties_page_model_load_from_file_info (NautilusImagePropertiesPageModel *page,
                                                                                  NautilusFileInfo                  *file_info);

NautilusImagePropertiesPageModel *nautilus_image_properties_page_model_new                 (void);
