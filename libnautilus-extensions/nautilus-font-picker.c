/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-font-picker.c - A simple widget to select scalable fonts.

   Copyright (C) 1999, 2000 Eazel, Inc.

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

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>

#include "nautilus-font-picker.h"

#include "nautilus-gtk-macros.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-art-gtk-extensions.h"
#include "nautilus-string.h"
#include "nautilus-font-manager.h"

#include <libgnome/gnome-i18n.h>

#include <gtk/gtkentry.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkradiomenuitem.h>
#include <gtk/gtksignal.h>

#include <libgnome/gnome-i18n.h>

typedef enum {
	FONT_SLANT_NORMAL,
	FONT_SLANT_OBLIQUE,
	FONT_SLANT_ITALIC
} FontSlant;

typedef enum {
	FONT_STRETCH_NORMAL,
	FONT_STRETCH_CONDENSED,
	FONT_STRETCH_SEMICONDENSED
} FontStretch;

typedef struct {
	char *name;
	gboolean is_bold;
	FontSlant slant;
	FontStretch stretch;
	char *font_file_name;
} FontStyleEntry;

typedef struct {
	char *name;
	char *family;
	char *name_for_display;
	GList *style_list;
} FontEntry;

typedef struct
{
	const char *name;
	const char *mapped;
} StyleMap;

static const char *normal_style = N_("Normal");
static const char *unknown_style = N_("Unknown");

static const StyleMap font_weight_map[] = {
       	{ "",	      NULL },
       	{ "medium",   NULL },
	{ "regular",  NULL },
	{ "bold",     N_("Bold") },
	{ "book",     N_("Book") },
	{ "black",    N_("Black") },
	{ "demibold", N_("Demibold") },
	{ "light",    N_("Light") }
};

static const StyleMap font_slant_map[] = {
       	{ "",	      NULL },
	{ "r",	      NULL },
	{ "i",	      N_("Italic"), },
	{ "o",	      N_("Oblique"), },
	{ "ri",	      N_("Reverse Italic"), },
	{ "ro",	      N_("Reverse Oblique"), },
	{ "ot",	      N_("Other"), }
};

static const StyleMap font_set_width_map[] = {
       	{ "",                 NULL },
	{ "normal",	      NULL },
	{ "condensed",	      N_("Condensed"), },
	{ "semicondensed",    N_("Semicondensed"), }
};

static const gint FONT_PICKER_SPACING = 10;

static GList *global_font_list = NULL;

#define FONT_MENU_ITEM_DATA_KEY "font-menu-entry"
#define FONT_MENU_STYLE_SUBMENU_DATA_KEY "font-menu-style-submenu"
#define FONT_STYLE_MENU_ITEM_DATA_KEY "font-style-menu-entry"
#define FONT_MENU_INDEX_DATA_KEY "font-menu-index"
#define FONT_MENU_IS_MORE_KEY "font-menu-is-more"

/* Signals */
typedef enum
{
	CHANGED,
	LAST_SIGNAL
} FontPickerSignals;

struct NautilusFontPickerDetails
{
	GtkWidget *option_menu;
	GtkWidget *menu;
	GtkWidget *current_menu;
	char *selected_font;
	gboolean option_menu_got_valid_selection;
};

/* GtkObjectClass methods */
static void                  nautilus_font_picker_initialize_class (NautilusFontPickerClass   *font_picker_class);
static void                  nautilus_font_picker_initialize       (NautilusFontPicker        *font_picker);
static void                  nautilus_font_picker_destroy          (GtkObject                 *object);

/* Private methods */
static void                  font_picker_populate                  (NautilusFontPicker        *font_picker);

/* Global font list */
static const GList *         global_font_list_get                  (void);
static void                  global_font_list_free                 (void);
static gboolean              global_font_list_populate_callback    (const char                *font_file_name,
								    NautilusFontType           font_type,
								    const char                *foundry,
								    const char                *family,
								    const char                *weight,
								    const char                *slant,
								    const char                *set_width,
								    const char                *char_set,
								    gpointer                   callback_data);
static guint                 nautilus_gtk_menu_shell_get_num_items (const GtkMenuShell        *menu_shell);
static const FontStyleEntry *font_picker_get_selected_style_entry  (const NautilusFontPicker  *font_picker);
static gboolean              font_picker_find_entries_for_font     (const char                *font_file_name,
								    const FontEntry          **entry,
								    const FontStyleEntry     **style_entry);
static int                   font_picker_get_index_for_entry       (const NautilusFontPicker  *font_picker,
								    const FontEntry           *entry);
	
NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusFontPicker, nautilus_font_picker, NAUTILUS_TYPE_CAPTION)

static guint font_picker_signals[LAST_SIGNAL] = { 0 };

/* GtkObjectClass methods */
static void
nautilus_font_picker_initialize_class (NautilusFontPickerClass *font_picker_class)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (font_picker_class);

	/* GtkObjectClass */
	object_class->destroy = nautilus_font_picker_destroy;

	/* Signals */
	font_picker_signals[CHANGED] = gtk_signal_new ("changed",
						       GTK_RUN_LAST,
						       object_class->type,
						       0,
						       gtk_marshal_NONE__NONE,
						       GTK_TYPE_NONE, 
						       0);

	gtk_object_class_add_signals (object_class, font_picker_signals, LAST_SIGNAL);
}

/* When the option menu is clicked, we reset a 'valid_selection' flag.
 * This flag will be set to TRUE when a valid selection is made.  We
 * need to do this to workaround a bug (or feature?) in GtkOptionMenu
 * that changes the selected item even if it wasnt actually clicked - for
 * example, if you changed your mind and clicked somewhere else after almost
 * selecting an item
 */
static int
option_menu_button_press_event (GtkButton *button,
				GdkEventButton *event,
				NautilusFontPicker *font_picker)
{
	g_return_val_if_fail (GTK_IS_BUTTON (button), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker), FALSE);

	font_picker->details->option_menu_got_valid_selection = FALSE;
	
	return FALSE;
}

/* If no valid selection was made, then we restore the old selected font */
static void
menu_deactivate (GtkMenuShell *menu_shell,
		 NautilusFontPicker *font_picker)
{
	g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));

	if (!font_picker->details->option_menu_got_valid_selection) {
		nautilus_font_picker_set_selected_font (font_picker,
							font_picker->details->selected_font);
	}

	/* This is needed to workaround a very annoying painting bug in
	 * menus.  In some themes (like the Eazel Crux theme) 'almost selected'
	 * items will appear white for some crazy reason.
	 */
	gtk_widget_queue_draw (GTK_WIDGET (menu_shell));
}

static void
nautilus_font_picker_initialize (NautilusFontPicker *font_picker)
{
	const FontStyleEntry *selected_style_entry;

	font_picker->details = g_new0 (NautilusFontPickerDetails, 1);
	gtk_box_set_homogeneous (GTK_BOX (font_picker), FALSE);
	gtk_box_set_spacing (GTK_BOX (font_picker), FONT_PICKER_SPACING);

	/* The font option menu */
	font_picker->details->option_menu = gtk_option_menu_new ();
	font_picker->details->menu = gtk_menu_new ();

	gtk_signal_connect (GTK_OBJECT (font_picker->details->option_menu),
			    "button_press_event",
			    GTK_SIGNAL_FUNC (option_menu_button_press_event),
			    font_picker);

	gtk_signal_connect (GTK_OBJECT (font_picker->details->menu),
			    "deactivate",
			    GTK_SIGNAL_FUNC (menu_deactivate),
			    font_picker);

	font_picker->details->current_menu = font_picker->details->menu;

	nautilus_caption_set_child (NAUTILUS_CAPTION (font_picker),
				    font_picker->details->option_menu,
				    FALSE,
				    FALSE);

	font_picker_populate (font_picker);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (font_picker->details->option_menu),
				  font_picker->details->menu);

	gtk_option_menu_set_history (GTK_OPTION_MENU (font_picker->details->option_menu), 0);

	selected_style_entry = font_picker_get_selected_style_entry  (font_picker);
	
	if (selected_style_entry != NULL) {
		font_picker->details->selected_font = g_strdup (selected_style_entry->font_file_name);
	}
}

/* GtkObjectClass methods */
static void
nautilus_font_picker_destroy (GtkObject* object)
{
	NautilusFontPicker *font_picker;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (object));
	
	font_picker = NAUTILUS_FONT_PICKER (object);

	g_free (font_picker->details->selected_font);
	g_free (font_picker->details);

	/* Chain */
	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static FontEntry *
font_list_find (const GList *font_list,
		const char *font_name)
{
	const GList *node;
	FontEntry *entry;

	/* First check the foundry qualified font names */
	for (node = font_list; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		entry = node->data;

		if (nautilus_istr_is_equal (font_name, entry->name)) {
			return entry;
		}
	}

	/* First check the foundry qualified font names */
	for (node = font_list; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		entry = node->data;

		if (nautilus_istr_is_equal (font_name, entry->family)) {
			return entry;
		}
	}

	return NULL;
}

static void
style_menu_item_activate_callback (GtkWidget *menu_item,
				   gpointer callback_data)
{
	const FontStyleEntry *style_entry;
	NautilusFontPicker *font_picker;

	g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));
	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (callback_data));

	font_picker = NAUTILUS_FONT_PICKER (callback_data);

	style_entry = gtk_object_get_data (GTK_OBJECT (menu_item), FONT_STYLE_MENU_ITEM_DATA_KEY);

	g_return_if_fail (style_entry != NULL);

	nautilus_font_picker_set_selected_font (font_picker, style_entry->font_file_name);
	
	gtk_signal_emit (GTK_OBJECT (font_picker), font_picker_signals[CHANGED]);
}

/* This event indicates that a valid selection was made in one of 
 * the style menus.
 */
static int
style_menu_item_button_release_event (GtkMenuItem *menu_item,
				      GdkEventButton *event,
				      NautilusFontPicker *font_picker)
{
 	g_return_val_if_fail (GTK_IS_MENU_ITEM (menu_item), FALSE);
 	g_return_val_if_fail (event != NULL, FALSE);
 	g_return_val_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker), FALSE);
	
	font_picker->details->option_menu_got_valid_selection = TRUE;
	
 	return FALSE;
}

static GtkWidget *
font_picker_add_item (NautilusFontPicker *font_picker,
		      const char *label,
		      int index,
		      const FontEntry *font_entry,
		      GtkWidget *style_menu)
{
	GtkWidget *item;
	NautilusDimensions item_dimensions;
	NautilusDimensions menu_dimensions;
	NautilusDimensions screen_dimensions;
	int available_height;
	int new_menu_height;
	int more_item_height_guess;
	GtkMenu *current_menu;

	g_return_val_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker), NULL);
	g_return_val_if_fail (label != NULL, NULL);
	g_return_val_if_fail (index >= 0, NULL);
	g_return_val_if_fail (font_entry != NULL, NULL);
	g_return_val_if_fail (GTK_IS_MENU (style_menu), NULL);
	
	current_menu = GTK_MENU (font_picker->details->current_menu);

	item = gtk_menu_item_new_with_label (label);
	
	gtk_object_set_data (GTK_OBJECT (item),
			     FONT_MENU_INDEX_DATA_KEY,
			     GINT_TO_POINTER (index));
	gtk_object_set_data (GTK_OBJECT (item),
			     FONT_MENU_ITEM_DATA_KEY,
			     (gpointer) font_entry);
	gtk_object_set_data (GTK_OBJECT (item),
			     FONT_MENU_STYLE_SUBMENU_DATA_KEY,
			     style_menu);

	screen_dimensions = nautilus_screen_get_dimensions ();
	menu_dimensions = nautilus_gtk_widget_get_preferred_dimensions (GTK_WIDGET (current_menu));
	item_dimensions = nautilus_gtk_widget_get_preferred_dimensions (item);

	/* FIXME: It would be cool if the available height took into
	 *        account the panel dimensions if its visible.  I have
	 *        no clue how to do that 
	 */
	available_height = screen_dimensions.height;

	/* We guess that the space needed by a possible "More..." item
	 * is the average height of the current items in the menu.
	 */
	more_item_height_guess = menu_dimensions.height / 
		MAX (1, nautilus_gtk_menu_shell_get_num_items (GTK_MENU_SHELL (current_menu)));

	/* Compute what the new menu height will be once the item is added.
	 * Always leave enough space to fit a "More..." menu, otherwise
	 * the "More..." menu itself would not fit in the screen.
	 */
	new_menu_height = menu_dimensions.height + item_dimensions.height + more_item_height_guess;

	/* If the item does not fit in the screen, create a "More..." submenu
	 * and put it there.  When Gtk+ 2.0 comes to town it will feature
	 * scrolling menus when needed and this hack wont be needed no more.
	 */
	if (new_menu_height >= available_height) {
		GtkWidget *more_item;
		GtkWidget *more_menu;

		more_item = gtk_menu_item_new_with_label (_("More..."));
		more_menu = gtk_menu_new ();

		gtk_menu_item_set_submenu (GTK_MENU_ITEM (more_item), more_menu);
		
		gtk_menu_append (GTK_MENU (font_picker->details->current_menu), more_item);
		
		gtk_widget_show (more_item);

		font_picker->details->current_menu = more_menu;
	}

	gtk_menu_append (GTK_MENU (font_picker->details->current_menu), item);

	/* If the item was added to a "More..." menu, then create a hidden
	 * duplicate item and add it to the main menu.  This is needed so that
	 * we dont break the way GtkOptionMenu "history" works.  In order to 
	 * be able to select menu items that exist in sub "More..." panes, the
	 * item has to exist in the main menu as well, otherwise the GtkOptionMenu
	 * refuses to set it as its selected item (which it calls "history")
	 */
	if (font_picker->details->current_menu != font_picker->details->menu) {
		GtkWidget *duplicate_hidden_item;

		duplicate_hidden_item = gtk_menu_item_new_with_label (label);
		
		gtk_object_set_data (GTK_OBJECT (duplicate_hidden_item),
				     FONT_MENU_INDEX_DATA_KEY,
				     GINT_TO_POINTER (index));
		gtk_object_set_data (GTK_OBJECT (duplicate_hidden_item),
				     FONT_MENU_ITEM_DATA_KEY,
				     (gpointer) font_entry);
		gtk_object_set_data (GTK_OBJECT (duplicate_hidden_item),
				     FONT_MENU_STYLE_SUBMENU_DATA_KEY,
				     style_menu);

		gtk_menu_append (GTK_MENU (font_picker->details->menu), duplicate_hidden_item);

		gtk_widget_hide (duplicate_hidden_item);
	}

	gtk_widget_show (item);

	return item;
}

static void
font_picker_populate (NautilusFontPicker *font_picker)
{
 	const GList *font_list;
 	const GList *node;
	const FontEntry *font_entry;
	const FontStyleEntry *style_entry;
	const GList *style_node;
	GtkWidget *font_menu_item;
	GtkWidget *style_menu_item = NULL;
	GtkWidget *style_menu;
	guint font_item_count;
	guint style_item_count;
	GSList *radio_item_group = NULL;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));

	font_list = global_font_list_get ();
	g_assert (font_list != NULL);

	for (node = font_list, font_item_count = 0;
	     node != NULL;
	     node = node->next, font_item_count++) {
		g_assert (node->data != NULL);
		font_entry = node->data;
		

		style_menu = gtk_menu_new ();
		font_menu_item = font_picker_add_item (font_picker,
						       font_entry->name_for_display,
						       font_item_count,
						       font_entry,
						       style_menu);

		gtk_menu_item_set_submenu (GTK_MENU_ITEM (font_menu_item), style_menu);

		for (style_node = font_entry->style_list, style_item_count = 0;
		     style_node != NULL;
		     style_node = style_node->next, style_item_count++) {
			g_assert (style_node->data != NULL);
			style_entry = style_node->data;

			radio_item_group = style_menu_item != NULL ? 
				gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (style_menu_item)) : 
				NULL;
			
			style_menu_item = gtk_radio_menu_item_new_with_label (radio_item_group, style_entry->name);

			gtk_menu_append (GTK_MENU (style_menu), style_menu_item);
			gtk_widget_show (style_menu_item);
			gtk_signal_connect (GTK_OBJECT (style_menu_item),
					    "activate",
					    GTK_SIGNAL_FUNC (style_menu_item_activate_callback),
					    font_picker);
			gtk_signal_connect (GTK_OBJECT (style_menu_item),
					    "button_release_event",
					    GTK_SIGNAL_FUNC (style_menu_item_button_release_event),
					    font_picker);
			gtk_object_set_data (GTK_OBJECT (style_menu_item),
					     FONT_STYLE_MENU_ITEM_DATA_KEY,
					     (gpointer) style_entry);
			gtk_object_set_data (GTK_OBJECT (style_menu_item),
					     FONT_MENU_INDEX_DATA_KEY,
					     GINT_TO_POINTER (style_item_count));
		}
	}
}

static const char *
font_find_style (const StyleMap *style_map,
		 guint num_styles,
		 const char *style)
{
	guint i;

	g_return_val_if_fail (style_map != NULL, NULL);
	g_return_val_if_fail (num_styles > 0, NULL);
	g_return_val_if_fail (style != NULL, NULL);

	for (i = 0; i < num_styles; i++) {
		if (nautilus_istr_is_equal (style, style_map[i].name)) {
			return _(style_map[i].mapped);
		}
	}

	return NULL;
}

static char *
font_make_name (const char *foundry,
		const char *family)
{
	g_return_val_if_fail (foundry != NULL, NULL);
	g_return_val_if_fail (family != NULL, NULL);

	return g_strdup_printf ("%s (%s)", family, foundry);
}

static char *
font_make_style_name (const char *weight,
		      const char *slant,
		      const char *set_width,
		      const char *char_set)
{
	const char *mapped_weight;
	const char *mapped_slant;
	const char *mapped_set_width;

	g_return_val_if_fail (weight != NULL, NULL);
	g_return_val_if_fail (slant != NULL, NULL);
	g_return_val_if_fail (set_width != NULL, NULL);
	g_return_val_if_fail (char_set != NULL, NULL);

	mapped_weight = font_find_style (font_weight_map,
					 NAUTILUS_N_ELEMENTS (font_weight_map),
					 weight);

	mapped_slant = font_find_style (font_slant_map,
					NAUTILUS_N_ELEMENTS (font_slant_map),
					slant);

	mapped_set_width = font_find_style (font_set_width_map,
					    NAUTILUS_N_ELEMENTS (font_set_width_map),
					    set_width);
	
	if (mapped_weight != NULL) {
		/* "abnormal" weight */
		if (mapped_slant == NULL && mapped_set_width == NULL) {
			return g_strdup (mapped_weight);
		} else if (mapped_slant != NULL && mapped_set_width != NULL) {
			return g_strdup_printf ("%s %s %s", mapped_weight, mapped_slant, mapped_set_width);
		} else if (mapped_slant != NULL) {
			return g_strdup_printf ("%s %s", mapped_weight, mapped_slant);
		}

		return g_strdup_printf ("%s %s", mapped_weight, mapped_set_width);
	} else {
		/* normal weight */
		if (mapped_slant == NULL && mapped_set_width == NULL) {
			return g_strdup (_(normal_style));
		} else if (mapped_slant != NULL && mapped_set_width != NULL) {
			return g_strdup_printf ("%s %s", mapped_slant, mapped_set_width);
		} else if (mapped_slant != NULL) {
			return g_strdup_printf ("%s", mapped_slant);
		}
		
		return g_strdup_printf ("%s", mapped_set_width);
	}

	return g_strdup (_(unknown_style));
}

static FontSlant
font_slant_string_to_enum (const char *slant)
{
	g_return_val_if_fail (slant != NULL, 0);

	if (nautilus_istr_is_equal (slant, "i")) {
		return FONT_SLANT_ITALIC;
	}

	if (nautilus_istr_is_equal (slant, "o")) {
		return FONT_SLANT_OBLIQUE;
	}

	return FONT_SLANT_NORMAL;
}

static FontStretch
font_set_width_string_to_enum (const char *set_width)
{
	g_return_val_if_fail (set_width != NULL, 0);

	if (nautilus_istr_is_equal (set_width, "condensed")) {
		return FONT_STRETCH_CONDENSED;
	}
	
	if (nautilus_istr_is_equal (set_width, "semicondensed")) {
		return FONT_STRETCH_SEMICONDENSED;
	}

	return FONT_STRETCH_NORMAL;
}

static FontStyleEntry *
font_style_entry_new (const char *font_file_name,
		      const char *weight,
		      const char *slant,
		      const char *set_width,
		      const char *char_set)
{
	FontStyleEntry *style_entry;

	g_return_val_if_fail (font_file_name != NULL, NULL);
	g_return_val_if_fail (weight != NULL, NULL);
	g_return_val_if_fail (slant != NULL, NULL);
	g_return_val_if_fail (set_width != NULL, NULL);
	g_return_val_if_fail (char_set != NULL, NULL);

	style_entry = g_new0 (FontStyleEntry, 1);
	style_entry->name = font_make_style_name (weight, slant, set_width, char_set);
	style_entry->is_bold = nautilus_font_manager_weight_is_bold (weight);
	style_entry->slant = font_slant_string_to_enum (slant);
	style_entry->stretch = font_set_width_string_to_enum (set_width);
	style_entry->font_file_name = g_strdup (font_file_name);

	return style_entry;
}

static void
font_style_entry_free (FontStyleEntry *style_entry)
{
	if (style_entry == NULL) {
		return;
	}

	g_free (style_entry->name);
	g_free (style_entry->font_file_name);
	g_free (style_entry);
}

/* Global font list */
static void
global_font_list_free (void)
{
 	GList *font_node;
	FontEntry *font_entry;
 	GList *style_node;
	FontStyleEntry *style_entry;

	if (global_font_list == NULL) {
		return;
	}

	for (font_node = global_font_list; font_node != NULL; font_node = font_node->next) {
		g_assert (font_node->data != NULL);
		font_entry = font_node->data;
		
		g_free (font_entry->name);
		g_free (font_entry->family);
		g_free (font_entry->name_for_display);

		for (style_node = font_entry->style_list; style_node != NULL; style_node = style_node->next) {
			g_assert (style_node->data != NULL);
			style_entry = style_node->data;
			font_style_entry_free (style_entry);
		}
		g_list_free (font_entry->style_list);

		g_free (font_entry);
	}

	global_font_list = NULL;
}

static guint
font_list_count_families (const GList *font_list,
			  const char *family)
{
	guint count = 0;
	const GList *node;
	const FontEntry *entry;

	g_return_val_if_fail (font_list != NULL, 0);
	g_return_val_if_fail (family != NULL, 0);
	
	for (node = font_list; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		entry = node->data;

		if (nautilus_istr_is_equal (family, entry->family)) {
			count++;
		}
	}

	return count;
}

static int
compare_font_entry (gconstpointer a,
		    gconstpointer b)
{
	const FontEntry *entry_a;
	const FontEntry *entry_b;

	g_return_val_if_fail (a != NULL, -1);
	g_return_val_if_fail (b != NULL, -1);

	entry_a = a;
	entry_b = b;

	return nautilus_strcasecmp_compare_func (entry_a->name_for_display, entry_b->name_for_display);
}

static const GList *
global_font_list_get (void)
{
	guint family_count = 0;
	GList *node;
	FontEntry *entry;

	if (global_font_list != NULL) {
		return global_font_list;
	}

	nautilus_font_manager_for_each_font (global_font_list_populate_callback,
					     &global_font_list);
	g_assert (global_font_list != NULL);

	for (node = global_font_list; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		entry = node->data;

		g_assert (entry->name_for_display == NULL);
		
		family_count = font_list_count_families (global_font_list, entry->family);
		g_assert (family_count > 0);
		
		entry->name_for_display = (family_count > 1) ? g_strdup (entry->name) : g_strdup (entry->family);
	}

	global_font_list = g_list_sort (global_font_list, compare_font_entry);

	g_atexit (global_font_list_free);

	return global_font_list;
}

static gboolean
list_contains_style (GList *styles, FontStyleEntry *style)
{
	GList *node;
	FontStyleEntry *existing_style;

	for (node = styles; node != NULL; node = node->next) {
		existing_style = node->data;
		/* If the files are the same, the fonts are the same font */
		if (strcmp (existing_style->font_file_name, style->font_file_name) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

#define EQUAL 0
#define LESS_THAN -1
#define GREATER_THAN 1

static int
compare_style (gconstpointer a,
	       gconstpointer b)
{
	const FontStyleEntry *style_a;
	const FontStyleEntry *style_b;

	g_return_val_if_fail (a != NULL, LESS_THAN);
	g_return_val_if_fail (b != NULL, LESS_THAN);

	style_a = a;
	style_b = b;

	/* Same weight */
	if (style_a->is_bold == style_b->is_bold) {
		/* Same slant */
		if (style_a->slant == style_b->slant) {
			return nautilus_compare_integer (GINT_TO_POINTER (style_a->stretch),
							 GINT_TO_POINTER (style_b->stretch));
		}

		return nautilus_compare_integer (GINT_TO_POINTER (style_a->slant),
						 GINT_TO_POINTER (style_b->slant));
	}

	/* Different weight */
	return style_b->is_bold ? LESS_THAN : GREATER_THAN;
}

static gboolean
global_font_list_populate_callback (const char *font_file_name,
				    NautilusFontType font_type,
				    const char *foundry,
				    const char *family,
				    const char *weight,
				    const char *slant,
				    const char *set_width,
				    const char *char_set,
				    gpointer callback_data)
{
	GList **font_list;
	char *entry_name;
	FontEntry *entry;
	FontStyleEntry *style_entry;

	g_return_val_if_fail (font_file_name != NULL, FALSE);
	g_return_val_if_fail (foundry != NULL, FALSE);
	g_return_val_if_fail (family != NULL, FALSE);
	g_return_val_if_fail (weight != NULL, FALSE);
	g_return_val_if_fail (slant != NULL, FALSE);
	g_return_val_if_fail (set_width != NULL, FALSE);
	g_return_val_if_fail (char_set != NULL, FALSE);
	g_return_val_if_fail (callback_data != NULL, FALSE);

	font_list = callback_data;

	entry_name = font_make_name (foundry, family);
	entry = font_list_find (*font_list, entry_name);

	if (entry == NULL) {
		entry = g_new0 (FontEntry, 1);
		entry->name = g_strdup (entry_name);
		entry->family = g_strdup (family);
		*font_list = g_list_append (*font_list, entry);
	}
	g_assert (entry != NULL);
	g_assert (font_list_find (*font_list, entry_name) == entry);
	g_free (entry_name);
	
	style_entry = font_style_entry_new (font_file_name,
					    weight,
					    slant,
					    set_width,
					    char_set);
	
	if (list_contains_style (entry->style_list, style_entry)) {
		font_style_entry_free (style_entry);
	} else {
		entry->style_list = g_list_insert_sorted (entry->style_list,
							  style_entry,
							  compare_style);
	}

	return TRUE;
}

static guint
nautilus_gtk_menu_shell_get_num_items (const GtkMenuShell *menu_shell)
{
	g_return_val_if_fail (GTK_IS_MENU_SHELL (menu_shell), 0);

	return g_list_length (menu_shell->children);
}

static const FontStyleEntry *
font_picker_get_selected_style_entry (const NautilusFontPicker *font_picker)
{
	GtkWidget *selected_font_menu_item;
	GtkMenu *style_submenu;
	GtkWidget *selected_style_menu_item;
	
	g_return_val_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker), NULL);

	selected_font_menu_item = GTK_OPTION_MENU (font_picker->details->option_menu)->menu_item;
	
	g_return_val_if_fail (GTK_IS_MENU_ITEM (selected_font_menu_item), NULL);
	
	style_submenu = gtk_object_get_data (GTK_OBJECT (selected_font_menu_item),
					     FONT_MENU_STYLE_SUBMENU_DATA_KEY);
	
	g_return_val_if_fail (GTK_IS_MENU (style_submenu), NULL);
	
	selected_style_menu_item = gtk_menu_get_active (style_submenu);
	g_return_val_if_fail (GTK_IS_MENU_ITEM (selected_style_menu_item), NULL);
	
	return gtk_object_get_data (GTK_OBJECT (selected_style_menu_item),
				    FONT_STYLE_MENU_ITEM_DATA_KEY);
}

static gboolean
font_picker_find_entries_for_font (const char *font_file_name,
				   const FontEntry **entry,
				   const FontStyleEntry **style_entry)
{
	const GList *font_list;
	const GList *font_list_node;
	const GList *style_list_node;

	g_return_val_if_fail (font_file_name != NULL, FALSE);
	g_return_val_if_fail (entry != NULL, FALSE);
	g_return_val_if_fail (style_entry != NULL, FALSE);
	
	font_list = global_font_list_get ();
	g_return_val_if_fail (font_list != NULL, FALSE);

	font_list_node = font_list;
	while (font_list_node != NULL) {
		g_assert (font_list_node->data != NULL);
		*entry = font_list_node->data;
		
		style_list_node = (*entry)->style_list;
		while (style_list_node != NULL) {
			g_assert (style_list_node->data != NULL);
			*style_entry = style_list_node->data;
			
			if (nautilus_istr_is_equal ((*style_entry)->font_file_name, font_file_name)) {
				return TRUE;
			}
			style_list_node = style_list_node->next;
		}
		
		font_list_node = font_list_node->next;
	}

	*entry = NULL;
	*style_entry = NULL;

	return FALSE;
}

static int
font_picker_get_index_for_entry (const NautilusFontPicker *font_picker,
				 const FontEntry *entry)
{
	GList *font_menu_node;
 	GtkWidget *font_menu_item;
	int i;

	g_return_val_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker), -1);
	g_return_val_if_fail (entry != NULL, -1);

	for (font_menu_node = GTK_MENU_SHELL (font_picker->details->menu)->children, i = 0;
	     font_menu_node != NULL;
	     font_menu_node = font_menu_node->next, i++) {
		g_return_val_if_fail (GTK_IS_MENU_ITEM (font_menu_node->data), -1);
		font_menu_item = font_menu_node->data;

		if (entry == gtk_object_get_data (GTK_OBJECT (font_menu_item), FONT_MENU_ITEM_DATA_KEY)) {
			return i;
		}
	}

	return -1;
}

/* NautilusFontPicker public methods */
GtkWidget *
nautilus_font_picker_new (void)
{
	return gtk_widget_new (nautilus_font_picker_get_type (), NULL);
}

char *
nautilus_font_picker_get_selected_font (const NautilusFontPicker *font_picker)
{
	g_return_val_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker), NULL);

	return g_strdup (font_picker->details->selected_font);
}

void
nautilus_font_picker_set_selected_font (NautilusFontPicker *font_picker,
					const char *font_name)
{
	int font_item_index;
	const FontEntry *entry;
	const FontStyleEntry *style_entry;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));

	if (!font_picker_find_entries_for_font (font_name, &entry, &style_entry)) {
		g_warning ("Trying to select a non existant font '%s'.", font_name);
		return;
	}
	
	g_assert (entry != NULL && style_entry != NULL);
	g_assert (nautilus_istr_is_equal (style_entry->font_file_name, font_name));

	font_item_index = font_picker_get_index_for_entry (font_picker, entry);

	g_return_if_fail (font_item_index != -1);

	if (!nautilus_istr_is_equal (font_picker->details->selected_font, font_name)) {
		g_free (font_picker->details->selected_font);
		font_picker->details->selected_font = g_strdup (font_name);
	}
	
	gtk_option_menu_set_history (GTK_OPTION_MENU (font_picker->details->option_menu),
				     font_item_index);
}
