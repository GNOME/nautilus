/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Andy Hertzfeld <andy@eazel.com>
 *          Ramiro Estrugo <ramiro@eazel.com>
 */

/* nautilus-theme-selector.h - Nautilus theme selector widget. */

#ifndef NAUTILUS_THEME_SELECTOR_H
#define NAUTILUS_THEME_SELECTOR_H

#include <gtk/gtkvbox.h>
#include <gtk/gtkwindow.h>
#include <libgnome/gnome-defs.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_THEME_SELECTOR (nautilus_theme_selector_get_type ())
#define NAUTILUS_THEME_SELECTOR(obj) (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_THEME_SELECTOR, NautilusThemeSelector))
#define NAUTILUS_THEME_SELECTOR_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_THEME_SELECTOR, NautilusThemeSelectorClass))
#define NAUTILUS_IS_THEME_SELECTOR(obj) (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_THEME_SELECTOR))
#define NAUTILUS_IS_THEME_SELECTOR_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_THEME_SELECTOR))

typedef struct NautilusThemeSelector		NautilusThemeSelector;
typedef struct NautilusThemeSelectorClass	NautilusThemeSelectorClass;
typedef struct NautilusThemeSelectorDetails	NautilusThemeSelectorDetails;

struct NautilusThemeSelector
{
	GtkVBox vbox;
	NautilusThemeSelectorDetails *details;
};

struct NautilusThemeSelectorClass
{
	GtkVBoxClass parent_class;
};

GtkType    nautilus_theme_selector_get_type           (void);
GtkWidget *nautilus_theme_selector_new                (void);
char *     nautilus_theme_selector_get_selected_theme (const NautilusThemeSelector *theme_selector);
void       nautilus_theme_selector_set_selected_theme (NautilusThemeSelector       *theme_selector,
						       char                        *theme_name);
/* Parent window for transient dialogs (file selector and error dialogs) */
void       nautilus_theme_selector_set_parent_window  (NautilusThemeSelector       *theme_selector,
						       GtkWindow                   *parent_window);

END_GNOME_DECLS

#endif /* NAUTILUS_THEME_SELECTOR_H */
