/* 
   rsvg-defs.c: Manage SVG defs and references.
 
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

#include <glib.h>
#include "rsvg-defs.h"

struct _RsvgDefs {
  GHashTable *hash;
};

RsvgDefs *
rsvg_defs_new (void)
{
  RsvgDefs *result = g_new (RsvgDefs, 1);

  result->hash = g_hash_table_new (g_str_hash, g_str_equal);

  return result;
}

RsvgDefVal *
rsvg_defs_lookup (const RsvgDefs *defs, const char *name)
{
  return (RsvgDefVal *)g_hash_table_lookup (defs->hash, name);
}

void
rsvg_defs_set (RsvgDefs *defs, const char *name, RsvgDefVal *val)
{
  g_hash_table_insert (defs->hash, g_strdup (name), val);
}

static void
rsvg_defs_free_each (gpointer key, gpointer value, gpointer user_data)
{
  RsvgDefVal *def_val = (RsvgDefVal *)value;
  g_free (key);
  def_val->free (def_val);
}

void
rsvg_defs_free (RsvgDefs *defs)
{
  g_hash_table_foreach (defs->hash, rsvg_defs_free_each, NULL);
  g_hash_table_destroy (defs->hash);
  g_free (defs);
}
