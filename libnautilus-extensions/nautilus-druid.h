/* nautilus-druid.h
 * Copyright (C) 1999  Red Hat, Inc.
 * Copyright (C) 2000  Eazel, Inc.
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/*
  @NOTATION@
*/
#ifndef NAUTILUS_DRUID_H
#define NAUTILUS_DRUID_H

#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-druid.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_DRUID            (nautilus_druid_get_type ())
#define NAUTILUS_DRUID(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_DRUID, NautilusDruid))
#define NAUTILUS_DRUID_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_DRUID, NautilusDruidClass))
#define NAUTILUS_IS_DRUID(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_DRUID))
#define NAUTILUS_IS_DRUID_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_DRUID))


typedef struct NautilusDruid        NautilusDruid;
typedef struct NautilusDruidClass   NautilusDruidClass;

struct NautilusDruid
{
	GnomeDruid gnome_druid;
};
struct NautilusDruidClass
{
	GnomeDruidClass parent_class;
};


GtkType    nautilus_druid_get_type              (void);
GtkWidget *nautilus_druid_new                   (void);

END_GNOME_DECLS

#endif /* NAUTILUS_DRUID_H */
