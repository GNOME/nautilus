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

void nautilus_bonobo_set_accelerator (BonoboUIComponent *ui_component,
				      const char        *path,
				      const char	*accelerator);

void nautilus_bonobo_set_description (BonoboUIComponent *ui_component,
				      const char        *path,
				      const char	*description);

void nautilus_bonobo_set_label (BonoboUIComponent *ui_component,
				const char        *path,
				const char	  *label);

void nautilus_bonobo_set_sensitive (BonoboUIComponent *ui_component,
				    const char        *path,
				    gboolean           sensitive);

void nautilus_bonobo_set_toggle_state (BonoboUIComponent *ui_component,
				       const char        *path,
				       gboolean           state);


void nautilus_bonobo_set_hidden (BonoboUIComponent *ui,
				 const char        *path,
				 gboolean           hidden);

gboolean nautilus_bonobo_get_hidden (BonoboUIComponent *ui,
				     const char        *path);

void	 nautilus_bonobo_add_menu_item (BonoboUIComponent *ui, 
					const char 	  *path, 
			 		const char 	  *label);

void	 nautilus_bonobo_remove_menu_items (BonoboUIComponent *ui,
				  	   const char 	     *path);

void nautilus_bonobo_set_icon (BonoboUIComponent *ui,
			       const char        *path,
			       const char        *icon_relative_path);


#ifdef UIH
void nautilus_bonobo_ui_handler_menu_set_toggle_appearance (BonoboUIHandler *uih,
				      	   		    const char      *path,
				      	   		    gboolean         new_value);
#endif

#endif /* NAUTILUS_BONOBO_EXTENSIONS_H */
