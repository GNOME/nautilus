/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
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
 * Author: Andy Hertzfeld <andy@eazel.com>
 */

/* This is the header file for the theme selector window, which
 * allows the user to select a Nautilus theme
 */

#ifndef NTL_THEME_SELECTOR_H
#define NTL_THEME_SELECTOR_H

#include <gtk/gtkwindow.h>

typedef struct NautilusThemeSelector NautilusThemeSelector;
typedef struct NautilusThemeSelectorClass  NautilusThemeSelectorClass;

#define NAUTILUS_TYPE_THEME_SELECTOR \
	(nautilus_theme_selector_get_type ())
#define NAUTILUS_THEME_SELECTOR(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_THEME_SELECTOR, NautilusThemeSelector))
#define NAUTILUS_THEME_SELECTOR_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_THEME_SELECTOR, NautilusThemeSelectorClass))
#define NAUTILUS_IS_THEME_SELECTOR(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_THEME_SELECTOR))
#define NAUTILUS_IS_THEME_SELECTOR_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_THEME_SELECTOR))

typedef struct NautilusThemeSelectorDetails NautilusThemeSelectorDetails;

struct NautilusThemeSelector
{
	GtkWindow window;
	NautilusThemeSelectorDetails *details;
};

struct NautilusThemeSelectorClass
{
	GtkWindowClass parent_class;
};

GtkType nautilus_theme_selector_get_type (void);
NautilusThemeSelector *nautilus_theme_selector_new (void);
void nautilus_theme_selector_show (void);

#endif /* NTL_THEME_SELECTOR_H */
