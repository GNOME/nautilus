/* 
   rsvg-path.h: Parse SVG path element data into bezier path.
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Raph Levien <raph@artofcode.com>
*/

#ifndef RSVG_PATH_H
#define RSVG_PATH_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

RsvgBpathDef *
rsvg_parse_path (const char *path_str);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
