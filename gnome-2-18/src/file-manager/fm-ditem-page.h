/*
 *  fm-ditem-page.h - A property page for desktop items
 * 
 *  Copyright (C) 2004 James Willcox
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: James Willcox <james@gnome.org>
 * 
 */

#ifndef FM_DITEM_PAGE_H
#define FM_DITEM_PAGE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define FM_TYPE_DITEM_PAGE  (fm_ditem_page_get_type ())
#define FM_DITEM_PAGE(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), FM_TYPE_DITEM_PAGE, FMDitemPage))
#define FM_IS_DITEM_PAGE(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), FM_TYPE_DITEM_PAGE))
typedef struct _FMDitemPage       FMDitemPage;
typedef struct _FMDitemPageClass  FMDitemPageClass;

struct _FMDitemPage {
	GObject parent_slot;
};

struct _FMDitemPageClass {
	GObjectClass parent_slot;
};

GType fm_ditem_page_get_type      (void);

G_END_DECLS

#endif
