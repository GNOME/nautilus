/* ntl-app.h
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.  */
#ifndef NTL_APP_H
#define NTL_APP_H 1

#include "ntl-window.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define NAUTILUS_TYPE_APP			(nautilus_app_get_type ())
#define NAUTILUS_APP(obj)			(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_APP, NautilusApp))
#define NAUTILUS_APP_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_APP, NautilusAppClass))
#define NAUTILUS_IS_APP(obj)			(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_APP))
#define NAUTILUS_IS_APP_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_APP))


typedef struct {
  BonoboObject parent;
  GSList *windows;
} NautilusApp;

typedef struct {
  BonoboObjectClass parent_class;
  gpointer servant;
  gpointer unknown_epv;
} NautilusAppClass;

GtkType    nautilus_app_get_type (void);
GtkObject *nautilus_app_new      (void);
void nautilus_app_startup(NautilusApp *app, const char *initial_url);
NautilusWindow *nautilus_app_create_window(NautilusApp *app);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
