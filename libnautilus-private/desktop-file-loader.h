/* desktop-file-loader.h 

   Copyright (C) 2001 Red Hat, Inc.

   Developers: Havoc Pennington <hp@redhat.com>
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   The library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place -
   Suite 330, Boston, MA 02111-1307, USA.

*/

#ifndef __DESKTOP_FILE_LOADER_H__
#define __DESKTOP_FILE_LOADER_H__

#include <glib.h>

typedef struct _DesktopFile DesktopFile;

/* No error handling here; should be added with GError on port
 * to GLib 2
 */

typedef void (* DesktopFileForeachFunc) (DesktopFile *df,
                                         const char *name,
                                         gpointer    data);

DesktopFile* desktop_file_new             (void);
DesktopFile* desktop_file_load            (const char             *filename);
DesktopFile* desktop_file_from_string     (const char             *data);
gboolean     desktop_file_save            (DesktopFile            *df,
                                           const char             *filename);
void         desktop_file_free            (DesktopFile            *df);
char**       desktop_file_get_lines       (DesktopFile            *df);
void         desktop_file_foreach_section (DesktopFile            *df,
                                           DesktopFileForeachFunc  func,
                                           gpointer                user_data);
void         desktop_file_foreach_key     (DesktopFile            *df,
                                           const char             *section,
                                           gboolean                include_localized,
                                           DesktopFileForeachFunc  func,
                                           gpointer                user_data);

/* This is crap, it just ignores the %f etc. in the exec string,
 *  and has no error handling.
 */
void         desktop_file_launch          (DesktopFile            *df);

gboolean desktop_file_get_boolean       (DesktopFile   *df,
                                         const char    *section,
                                         const char    *keyname,
                                         gboolean      *val);
gboolean desktop_file_get_number        (DesktopFile   *df,
                                         const char    *section,
                                         const char    *keyname,
                                         double        *val);
gboolean desktop_file_get_string        (DesktopFile   *df,
                                         const char    *section,
                                         const char    *keyname,
                                         char         **val);
gboolean desktop_file_get_locale_string  (DesktopFile   *df,
                                          const char    *section,
                                          const char    *keyname,
                                          char         **val);
gboolean desktop_file_get_regexp        (DesktopFile   *df,
                                         const char    *section,
                                         const char    *keyname,
                                         char         **val);
gboolean desktop_file_get_booleans      (DesktopFile   *df,
                                         const char    *section,
                                         const char    *keyname,
                                         gboolean     **vals,
                                         int           *len);
gboolean desktop_file_get_numbers       (DesktopFile   *df,
                                         const char    *section,
                                         const char    *keyname,
                                         double       **vals,
                                         int           *len);
gboolean desktop_file_get_strings       (DesktopFile   *df,
                                         const char    *section,
                                         const char    *keyname,
                                         char        ***vals,
                                         int           *len);
gboolean desktop_file_get_locale_strings (DesktopFile   *df,
                                          const char    *section,
                                          const char    *keyname,
                                          char        ***vals,
                                          int           *len);
gboolean desktop_file_get_regexps       (DesktopFile   *df,
                                         const char    *section,
                                         const char    *keyname,
                                         char        ***vals,
                                         int           *len);

gboolean desktop_file_set_string (DesktopFile *df,
				  const char  *section,
				  const char  *keyname,
				  const char  *value);


#endif /* __DESKTOP_FILE_LOADER_H__ */
