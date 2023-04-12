
/* nautilus-file-operations: execute file operations.

   Copyright (C) 1999, 2000 Free Software Foundation
   Copyright (C) 2000, 2001 Eazel, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
   
   You should have received a copy of the GNU General Public
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
   
   Authors: Ettore Perazzoli <ettore@gnu.org>,
            Pavel Cisler <pavel@eazel.com>
*/

#pragma once

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <gnome-autoar/gnome-autoar.h>

#include "nautilus-file-operations-dbus-data.h"

#define SECONDS_NEEDED_FOR_APROXIMATE_TRANSFER_RATE 1

typedef void (* NautilusCopyCallback)      (GHashTable *debuting_uris,
					    gboolean    success,
					    gpointer    callback_data);
typedef void (* NautilusCreateCallback)    (GFile      *new_file,
					    gboolean    success,
					    gpointer    callback_data);
typedef void (* NautilusOpCallback)        (gboolean    success,
					    gpointer    callback_data);
typedef void (* NautilusDeleteCallback)    (GHashTable *debuting_uris,
					    gboolean    user_cancel,
					    gpointer    callback_data);
typedef void (* NautilusMountCallback)     (GVolume    *volume,
					    gboolean    success,
					    GObject    *callback_data_object);
typedef void (* NautilusUnmountCallback)   (gpointer    callback_data);
typedef void (* NautilusExtractCallback)   (GList    *outputs,
                                            gpointer  callback_data);

/* FIXME: int copy_action should be an enum */

void nautilus_file_operations_copy_move   (const GList                    *item_uris,
                                           const char                     *target_dir_uri,
                                           GdkDragAction                   copy_action,
                                           GtkWidget                      *parent_view,
                                           NautilusFileOperationsDBusData *dbus_data,
                                           NautilusCopyCallback            done_callback,
                                           gpointer                        done_callback_data);
void nautilus_file_operations_empty_trash (GtkWidget                      *parent_view,
                                           gboolean                        ask_confirmation,
                                           NautilusFileOperationsDBusData *dbus_data);
void nautilus_file_operations_new_folder  (GtkWidget                      *parent_view,
                                           NautilusFileOperationsDBusData *dbus_data,
                                           const char                     *parent_dir_uri,
                                           const char                     *folder_name,
                                           NautilusCreateCallback          done_callback,
                                           gpointer                        done_callback_data);
void nautilus_file_operations_new_file    (GtkWidget                 *parent_view,
					   const char                *parent_dir,
					   const char                *target_filename,
					   const char                *initial_contents,
					   gsize                      length,
					   NautilusCreateCallback     done_callback,
					   gpointer                   data);
void nautilus_file_operations_new_file_from_template (GtkWidget               *parent_view,
						      const char              *parent_dir,
						      const char              *target_filename,
						      const char              *template_uri,
						      NautilusCreateCallback   done_callback,
						      gpointer                 data);

void nautilus_file_operations_trash_or_delete_sync (GList                  *files);
void nautilus_file_operations_delete_sync (GList                  *files);
void nautilus_file_operations_trash_or_delete_async (GList                          *files,
                                                     GtkWindow                      *parent_window,
                                                     NautilusFileOperationsDBusData *dbus_data,
                                                     NautilusDeleteCallback          done_callback,
                                                     gpointer                        done_callback_data);
void nautilus_file_operations_delete_async (GList                          *files,
                                            GtkWindow                      *parent_window,
                                            NautilusFileOperationsDBusData *dbus_data,
                                            NautilusDeleteCallback          done_callback,
                                            gpointer                        done_callback_data);

void nautilus_file_set_permissions_recursive (const char                     *directory,
					      guint32                         file_permissions,
					      guint32                         file_mask,
					      guint32                         folder_permissions,
					      guint32                         folder_mask,
					      NautilusOpCallback              callback,
					      gpointer                        callback_data);

void nautilus_file_operations_unmount_mount (GtkWindow                      *parent_window,
					     GMount                         *mount,
					     gboolean                        eject,
					     gboolean                        check_trash);
void nautilus_file_operations_unmount_mount_full (GtkWindow                 *parent_window,
						  GMount                    *mount,
						  GMountOperation           *mount_operation,
						  gboolean                   eject,
						  gboolean                   check_trash,
						  NautilusUnmountCallback    callback,
						  gpointer                   callback_data);
void nautilus_file_operations_mount_volume  (GtkWindow                      *parent_window,
					     GVolume                        *volume);
void nautilus_file_operations_mount_volume_full (GtkWindow                      *parent_window,
						 GVolume                        *volume,
						 NautilusMountCallback           mount_callback,
						 GObject                        *mount_callback_data_object);

void nautilus_file_operations_copy_async (GList                          *files,
                                          GFile                          *target_dir,
                                          GtkWindow                      *parent_window,
                                          NautilusFileOperationsDBusData *dbus_data,
                                          NautilusCopyCallback            done_callback,
                                          gpointer                        done_callback_data);
void nautilus_file_operations_copy_sync (GList                *files,
                                         GFile                *target_dir);

void nautilus_file_operations_move_async (GList                          *files,
                                          GFile                          *target_dir,
                                          GtkWindow                      *parent_window,
                                          NautilusFileOperationsDBusData *dbus_data,
                                          NautilusCopyCallback            done_callback,
                                          gpointer                        done_callback_data);
void nautilus_file_operations_move_sync (GList                *files,
                                         GFile                *target_dir);

void nautilus_file_operations_duplicate (GList                          *files,
                                         GtkWindow                      *parent_window,
                                         NautilusFileOperationsDBusData *dbus_data,
                                         NautilusCopyCallback            done_callback,
                                         gpointer                        done_callback_data);
void nautilus_file_operations_link      (GList                          *files,
                                         GFile                          *target_dir,
                                         GtkWindow                      *parent_window,
                                         NautilusFileOperationsDBusData *dbus_data,
                                         NautilusCopyCallback            done_callback,
                                         gpointer                        done_callback_data);
void nautilus_file_operations_extract_files (GList                          *files,
                                             GFile                          *destination_directory,
                                             GtkWindow                      *parent_window,
                                             NautilusFileOperationsDBusData *dbus_data,
                                             NautilusExtractCallback         done_callback,
                                             gpointer                        done_callback_data);
void nautilus_file_operations_compress (GList                          *files,
                                        GFile                          *output,
                                        AutoarFormat                    format,
                                        AutoarFilter                    filter,
                                        const gchar                    *passphrase,
                                        GtkWindow                      *parent_window,
                                        NautilusFileOperationsDBusData *dbus_data,
                                        NautilusCreateCallback          done_callback,
                                        gpointer                        done_callback_data);

void
nautilus_file_operations_paste_image_from_clipboard (GtkWidget                      *parent_view,
                                                    NautilusFileOperationsDBusData *dbus_data,
                                                    const char                     *parent_dir_uri,
                                                    NautilusCopyCallback            done_callback,
                                                    gpointer                        done_callback_data);

void
nautilus_file_operations_save_image_from_texture (GtkWidget                      *parent_view,
                                                  NautilusFileOperationsDBusData *dbus_data,
                                                  const char                     *parent_dir_uri,
                                                  const char                     *base_name,
                                                  GdkTexture                     *texture,
                                                  NautilusCopyCallback            done_callback,
                                                  gpointer                        done_callback_data);
