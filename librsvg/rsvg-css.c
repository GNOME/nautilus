/* 
   rsvg-css.c: Parse CSS basic data types.
 
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

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <glib.h>

#include "rsvg-css.h"

/**
 * rsvg_css_parse_length: Parse CSS2 length to a pixel value.
 * @str: Original string.
 * @fixed: Where to store boolean value of whether length is fixed.
 *
 * Parses a CSS2 length into a pixel value.
 *
 * Returns: returns the length.
 **/
double
rsvg_css_parse_length (const char *str, gint *fixed)
{
  char *p;
  
  /* 
   *  The supported CSS length unit specifiers are: 
   *  em, ex, px, pt, pc, cm, mm, in, and percentages. 
   */
  
  *fixed = FALSE;

  p = strstr (str, "px");
  if (p != NULL)
    {
      *fixed = TRUE;
      return atof (str);
    }
  p = strstr (str, "in");
  if (p != NULL)
    {
      *fixed = TRUE;
      /* return svg->pixels_per_inch * atof (str); */
    }
  p = strstr (str, "%");
  if (p != NULL)
    {
      return 0.01 * atof (str);
    }
  return atof (str);
}

gboolean
rsvg_css_param_match (const char *str, const char *param_name)
{
  int i;

  for (i = 0; str[i] != '\0' && str[i] != ':'; i++)
    if (param_name[i] != str[i])
      return FALSE;
  return str[i] == ':' && param_name[i] == '\0';
}

int
rsvg_css_param_arg_offset (const char *str)
{
  int i;

  for (i = 0; str[i] != '\0' && str[i] != ':'; i++);
  if (str[i] != '\0') i++;
  for (; str[i] == ' '; i++);
  return i;
}

/* Parse a CSS2 color, returning rgb */
guint32
rsvg_css_parse_color (const char *str)
{
  gint val = 0;
  static GHashTable *colors = NULL;

  /* todo: better failure detection */

  /* 
   * todo: handle the rgb (r, g, b) and rgb ( r%, g%, b%), syntax 
   * defined in http://www.w3.org/TR/REC-CSS2/syndata.html#color-units 
   */
#ifdef VERBOSE
  g_print ("color = %s\n", str);
#endif
  if (str[0] == '#')
    {
      int i;
      for (i = 1; str[i]; i++)
	{
	  int hexval;
	  if (str[i] >= '0' && str[i] <= '9')
	    hexval = str[i] - '0';
	  else if (str[i] >= 'A' && str[i] <= 'F')
	    hexval = str[i] - 'A' + 10;
	  else if (str[i] >= 'a' && str[i] <= 'f')
	    hexval = str[i] - 'a' + 10;
	  else
	    break;
	  val = (val << 4) + hexval;
	}
      /* handle #rgb case */
      if (i == 4)
	{
	  val = ((val & 0xf00) << 8) |
	    ((val & 0x0f0) << 4) |
	    (val & 0x00f);
	  val |= val << 4;
	}
#ifdef VERBOSE
      printf ("val = %x\n", val);
#endif
    } 
  else
    {
      GString * string;
      if (!colors)
	{
	  colors = g_hash_table_new (g_str_hash, g_str_equal);
	  
	  g_hash_table_insert (colors, "black",    GINT_TO_POINTER (0x000000));
	  g_hash_table_insert (colors, "silver",   GINT_TO_POINTER (0xc0c0c0));
	  g_hash_table_insert (colors, "gray",     GINT_TO_POINTER (0x808080));
	  g_hash_table_insert (colors, "white",    GINT_TO_POINTER (0xFFFFFF));
	  g_hash_table_insert (colors, "maroon",   GINT_TO_POINTER (0x800000));
	  g_hash_table_insert (colors, "red",      GINT_TO_POINTER (0xFF0000));
	  g_hash_table_insert (colors, "purple",   GINT_TO_POINTER (0x800080));
	  g_hash_table_insert (colors, "fuchsia",  GINT_TO_POINTER (0xFF00FF));
	  g_hash_table_insert (colors, "green",    GINT_TO_POINTER (0x008000));
	  g_hash_table_insert (colors, "lime",     GINT_TO_POINTER (0x00FF00));
	  g_hash_table_insert (colors, "olive",    GINT_TO_POINTER (0x808000));
	  g_hash_table_insert (colors, "yellow",   GINT_TO_POINTER (0xFFFF00));
	  g_hash_table_insert (colors, "navy",     GINT_TO_POINTER (0x000080));
	  g_hash_table_insert (colors, "blue",     GINT_TO_POINTER (0x0000FF));
	  g_hash_table_insert (colors, "teal",     GINT_TO_POINTER (0x008080));
	  g_hash_table_insert (colors, "aqua",     GINT_TO_POINTER (0x00FFFF));
	}

      string = g_string_down (g_string_new (str));

      /* this will default to black on a failed lookup */
      val = GPOINTER_TO_INT (g_hash_table_lookup (colors, string->str)); 
    }

  return val;
}

guint
rsvg_css_parse_opacity (const char *str)
{
  char *end_ptr;
  double opacity;

  opacity = strtod (str, &end_ptr);

  if (end_ptr[0] == '%')
    opacity *= 0.01;

  return floor (opacity * 255 + 0.5);
}

double
rsvg_css_parse_fontsize (const char *str)
{
  char *end_ptr;
  double size;

  /* todo: handle absolute-size and relative-size tags and proper units */
  size = strtod (str, &end_ptr);

  if (end_ptr[0] == '%')
    size = (36 * size * 0.01); /* todo: egregious hack */

  return size;
}

