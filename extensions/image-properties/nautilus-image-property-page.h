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

#ifndef NAUTILUS_IMAGE_PROPERTY_PAGE_H
#define NAUTILUS_IMAGE_PROPERTY_PAGE_H

#include <gtk/gtk.h>

#include <nautilus-extension.h>

#define NAUTILUS_TYPE_IMAGE_PROPERTY_PAGE (nautilus_image_property_page_get_type ())

G_DECLARE_FINAL_TYPE (NautilusImagePropertyPage,
                      nautilus_image_property_page,
                      NAUTILUS, IMAGE_PROPERTY_PAGE,
                      GtkGrid)

void                       nautilus_image_property_page_load_from_file_info (NautilusImagePropertyPage *page,
                                                                             NautilusFileInfo          *file_info);

NautilusImagePropertyPage *nautilus_image_property_page_new                 (void);

#endif
