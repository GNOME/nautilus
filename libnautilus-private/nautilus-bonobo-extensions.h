/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-bonobo-extensions.h - interface for new functions that conceptually
                                  belong in bonobo. Perhaps some of these will be
                                  actually rolled into bonobo someday.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: John Sullivan <sullivan@eazel.com>
*/

#ifndef NAUTILUS_BONOBO_EXTENSIONS_H
#define NAUTILUS_BONOBO_EXTENSIONS_H

#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-xobject.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

typedef struct NautilusBonoboActivationHandle NautilusBonoboActivationHandle;

typedef void (*NautilusBonoboActivationCallback) (NautilusBonoboActivationHandle *handle,
						  Bonobo_Unknown activated_object,
						  gpointer callback_data);

void                            nautilus_bonobo_set_accelerator                     (BonoboUIComponent                *ui,
										     const char                       *path,
										     const char                       *accelerator);
char *                          nautilus_bonobo_get_label                           (BonoboUIComponent                *ui,
										     const char                       *path);
void                            nautilus_bonobo_set_label                           (BonoboUIComponent                *ui,
										     const char                       *path,
										     const char                       *label);
void                            nautilus_bonobo_set_tip                             (BonoboUIComponent                *ui,
										     const char                       *path,
										     const char                       *tip);
void                            nautilus_bonobo_set_sensitive                       (BonoboUIComponent                *ui,
										     const char                       *path,
										     gboolean                          sensitive);
void                            nautilus_bonobo_set_toggle_state                    (BonoboUIComponent                *ui,
										     const char                       *path,
										     gboolean                          state);
void                            nautilus_bonobo_set_hidden                          (BonoboUIComponent                *ui,
										     const char                       *path,
										     gboolean                          hidden);
gboolean                        nautilus_bonobo_get_hidden                          (BonoboUIComponent                *ui,
										     const char                       *path);
void                            nautilus_bonobo_add_numbered_menu_item              (BonoboUIComponent                *ui,
										     const char                       *container_path,
										     guint                             index,
										     const char                       *label,
										     GdkPixbuf                        *pixbuf);
void                            nautilus_bonobo_add_numbered_toggle_menu_item       (BonoboUIComponent                *ui,
										     const char                       *container_path,
										     guint                             index,
										     const char                       *label);
void                            nautilus_bonobo_add_numbered_radio_menu_item        (BonoboUIComponent                *ui,
										     const char                       *container_path,
										     guint                             index,
										     const char                       *label,
										     const char			      *radio_group_name);
char *                          nautilus_bonobo_get_numbered_menu_item_command      (BonoboUIComponent                *ui,
										     const char                       *container_path,
										     guint                             index);
char *                          nautilus_bonobo_get_numbered_menu_item_path         (BonoboUIComponent                *ui,
										     const char                       *container_path,
										     guint                             index);
guint			        nautilus_bonobo_get_numbered_menu_item_index_from_command
										    (const char 		      *command);
char *			        nautilus_bonobo_get_numbered_menu_item_container_path_from_command
										    (const char 		      *command);
void                            nautilus_bonobo_add_submenu                         (BonoboUIComponent                *ui,
										     const char                       *container_path,
										     const char                       *label);
void                            nautilus_bonobo_add_menu_separator                  (BonoboUIComponent                *ui,
										     const char                       *path);
void                            nautilus_bonobo_remove_menu_items_and_commands      (BonoboUIComponent                *ui,
										     const char                       *container_path);
void                            nautilus_bonobo_set_label_for_menu_item_and_command (BonoboUIComponent                *ui,
										     const char                       *menu_item_path,
										     const char                       *command_path,
										     const char                       *label_with_underscore);
void                            nautilus_bonobo_set_icon                            (BonoboUIComponent                *ui,
										     const char                       *path,
										     const char                       *icon_relative_path);

NautilusBonoboActivationHandle *nautilus_bonobo_activate_from_id                    (const char                       *iid,
										     NautilusBonoboActivationCallback  callback,
										     gpointer                          callback_data);
void                            nautilus_bonobo_activate_cancel                     (NautilusBonoboActivationHandle   *handle);


/* This macro is a copy of BONOBO_X_TYPE_FUNC_FULL (from bonobo-xobject.h)
 * with the addition of support for the parent_class which is defined by
 * EEL_DEFINE_CLASS_BOILERPLATE and used by EEL_CALL_PARENT.
 * 
 * Note: the argument order matches BONOBO_X_TYPE_FUNC_FULL which is different
 * than EEL_DEFINE_CLASS_BOILERPLATE.
 */
#define NAUTILUS_BONOBO_X_BOILERPLATE(class_name, corba_name, parent, prefix) \
static gpointer parent_class;                          /* Nautilus change */  \
GtkType                                                                       \
prefix##_get_type (void)                                                      \
{                                                                             \
	GtkType ptype;                                                        \
	static GtkType type = 0;                                              \
                                                                              \
	if (type == 0) {                                                      \
		static GtkTypeInfo info = {                                   \
			#class_name,                                          \
			sizeof (class_name),                                  \
			sizeof (class_name##Class),                           \
			(GtkClassInitFunc)prefix##_class_init,                \
			(GtkObjectInitFunc)prefix##_init,                     \
			NULL, NULL, (GtkClassInitFunc) NULL                   \
		};                                                            \
		ptype = (parent);                                             \
		type = bonobo_x_type_unique (ptype,                           \
			POA_##corba_name##__init, POA_##corba_name##__fini,   \
			GTK_STRUCT_OFFSET (class_name##Class, epv),           \
			&info);                                               \
		parent_class = gtk_type_class (ptype); /* Nautilus change */  \
	}                                                                     \
	return type;                                                          \
}


#endif /* NAUTILUS_BONOBO_EXTENSIONS_H */
