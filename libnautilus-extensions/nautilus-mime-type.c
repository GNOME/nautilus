/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 

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
   
   Here are the routines that return applications that are relevant to a given MIME-type.
   For now, the info is kept in a statically defined string table, but it eventually has to
   be editable and augmentable.  We'll probably use OAF or GConf for this, but at least this
   lets us get going with the UI for now.
   
   Author: Andy Hertzfeld <andy@eazel.com>
*/

#include <config.h>
#include <string.h>
#include <stdio.h>
#include "nautilus-glib-extensions.h"

#include "nautilus-mime-type.h"

typedef struct {
	gchar *base_type;
	gchar *sub_type;
	gchar *display_name;
	gchar *command_string;
} MimeTypeItem;

/* here is the string table associating mime types with commands */
/* FIXME bugzilla.eazel.com 586:  this should be kept in a file somewhere, possibly using GConf or OAF */

static MimeTypeItem mime_type_table [] = {
	{ "text", "plain",  "gEdit", "gedit" },
	{ "text", "html", "Netscape", "netscape" },
	{ "image", "*", "GIMP",  "gimp" },
	{ "text", "*",  "gnotepad", "gnp" }
};

/* release the storage contained in the passed-in command list */

void nautilus_mime_type_dispose_list (GList *command_list)
{
  GList *next_command;
  for (next_command = command_list; next_command != NULL; next_command = next_command->next)
    {
      NautilusCommandInfo *item = (NautilusCommandInfo *) next_command->data;
     
      g_free(item->display_name);
      g_free(item->command_string);     
      g_free(item);
    }   
  
  g_list_free (command_list);
}

/* return a list of commands corresponding to the passed in mime-type, by iterating
   through the table */

GList* nautilus_mime_type_get_commands (const gchar *mime_type)
{
  gint index;
  gchar *slash_pos, *temp_str;
  gchar *target_base_type, *target_sub_type;
  NautilusCommandInfo *new_command_item;
  GList *command_list = NULL;
  
  /* parse the mime type into a base type and a sub type */
  
  temp_str = strdup(mime_type);
  
  target_base_type = temp_str;
  slash_pos = strchr(temp_str, '/');
  if (slash_pos)
    {
      *slash_pos = '\0';
       target_sub_type = slash_pos + 1;
    }
  else
       target_sub_type = NULL;
           
  /* iterate through the table, creating a new command info node for each mime type that matches */
  
  for (index = 0; index < NAUTILUS_N_ELEMENTS(mime_type_table); index++)
    {
      /* see if the types match */
      
      if (strcmp(mime_type_table[index].base_type, target_base_type) == 0)
      	{
	  gchar *cur_sub_type = mime_type_table[index].sub_type;
	  if ((target_sub_type == NULL) || (strcmp(cur_sub_type, target_sub_type) == 0) || (strcmp(cur_sub_type, "*") == 0))
	    {
      		/* the types match, so allocate a command entry */
      		new_command_item = g_new0 (NautilusCommandInfo, 1);
 
      		/* add it to the list */
      		if (command_list != NULL)
 		  command_list = g_list_append(command_list, new_command_item);
      		else
        	  { 
          	    command_list = g_list_alloc(); 
          	    command_list->data = new_command_item;
        	  } 
		    
		new_command_item->display_name = strdup(mime_type_table[index].display_name);
		new_command_item->command_string = strdup(mime_type_table[index].command_string);		    
    	    }
         }
    }
  
  g_free(temp_str);
  return command_list;
}


