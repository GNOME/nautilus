
#include <config.h>

#include "test.h"

#include <libnautilus-extensions/nautilus-font-manager.h>

static const char *
font_type_to_string (NautilusFontType font_type)
{
	if (font_type == NAUTILUS_FONT_POSTSCRIPT) {
		return "postscript";
	}

	if (font_type == NAUTILUS_FONT_TRUE_TYPE) {
		return "truetype";
	}

	return "unknown";
}

// microsoft Webdings
// microsoft Wingdings
// monotype OCR
// URW Zapf
// xfree86 cursor

typedef struct
{
	char *key;
	char *font_file_name;
} Style;

typedef struct
{
	char *title;
	char *foundry;
	char *family;
	char *key;
	GList *style_list;
} Entry;

static gboolean
font_iterator_callback (const char *font_file_name,
			NautilusFontType font_type,
			const char *foundry,
			const char *family,
			const char *weight,
			const char *slant,
			const char *set_width,
			const char *char_set_registry,
			const char *char_set_encoding,
			gpointer callback_data)
{
	char *key;
	Entry *entry;
	Style *style;

	GHashTable *font_table;

	g_return_val_if_fail (font_file_name != NULL, FALSE);
	g_return_val_if_fail (foundry != NULL, FALSE);
	g_return_val_if_fail (family != NULL, FALSE);
	g_return_val_if_fail (weight != NULL, FALSE);
	g_return_val_if_fail (slant != NULL, FALSE);
	g_return_val_if_fail (set_width != NULL, FALSE);
	g_return_val_if_fail (char_set_registry != NULL, FALSE);
	g_return_val_if_fail (char_set_encoding != NULL, FALSE);
	g_return_val_if_fail (callback_data != NULL, FALSE);

	font_table = callback_data;

	key = g_strdup_printf ("%s %s", foundry, family);

	entry = g_hash_table_lookup (font_table, key);

	if (entry == NULL) {
		entry = g_new0 (Entry, 1);
		entry->key = g_strdup (key);
		entry->foundry = g_strdup (foundry);
		entry->family = g_strdup (family);

		g_hash_table_insert (font_table, entry->key, entry);
	}
	g_assert (entry != NULL);
	g_assert (g_hash_table_lookup (font_table, key) == entry);
	g_free (key);

	style = g_new0 (Style, 1);
	style->key = g_strdup_printf ("%s %s %s", weight, slant, set_width);
	style->font_file_name = g_strdup (font_file_name);

	entry->style_list = g_list_append (entry->style_list, style);

	if (1) {
		g_print ("%s %s %s-%s-%s-%s-%s-%s-%s\n",
			 font_type_to_string (font_type),
			 font_file_name,
			 foundry,
			 family,
			 weight,
			 slant,
			 set_width,
			 char_set_registry,
			 char_set_encoding);
	}

	return TRUE;
}

static void
font_table_for_each_callback (gpointer key,
			      gpointer value,
			      gpointer callback_data)
{
	char *entry_key;

	g_return_if_fail (key != NULL);
	g_return_if_fail (value != NULL);
	//g_return_if_fail (callback_data != NULL);

	entry_key = key;

	if (0) g_print ("%s\n", entry_key);
}

int 
main (int argc, char* argv[])
{
	GHashTable *font_table = NULL;
	
	test_init (&argc, &argv);

	font_table = g_hash_table_new (g_str_hash, g_str_equal);

	nautilus_font_manager_for_each_font (font_iterator_callback,
					     font_table);

	g_hash_table_foreach (font_table, font_table_for_each_callback, NULL);
	
	return test_quit (EXIT_SUCCESS);
}
