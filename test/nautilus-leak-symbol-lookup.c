/* nautilus-leak-symbol-lookup.c - symbol lookup for a leak checking library
   Virtual File System Library

   Copyright (C) 2000 Eazel

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

   Author: Pavel Cisler <pavel@eazel.com>
   based on MemProf by Owen Taylor, <otaylor@redhat.com>
*/

#define _GNU_SOURCE	
	/* need this for dladdr */

#include "nautilus-leak-symbol-lookup.h"

#include <bfd.h>
#include <dlfcn.h>
#include <glib.h>
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/stat.h>


static GList *symbol_table_list;

typedef struct {
	char *path;
	bfd *abfd;
	asymbol **symbol_table;
	asection *text_section;
} NautilusLeakSymbolLookupMap;

static gboolean
nautilus_leak_find_symbol_in_map (const NautilusLeakSymbolLookupMap *map, 
	unsigned long address, char **function_name, char **source_file_name, 
	unsigned int *line, unsigned long offset, const char *known_function_name)
{
	const char *file;
	const char *function;

	address -= offset;
	address -= map->text_section->vma;

	if (address < 0 || address > map->text_section->_cooked_size) {
		/* not a valid address range for this binary */
		return FALSE;
	}
	
	if (!bfd_find_nearest_line (map->abfd, map->text_section, map->symbol_table, 
				    address,
				    &file, &function, line)) {
		printf ("error looking up address in binary %s\n", map->path);
		return FALSE;
	}

	if (known_function_name != NULL &&
		strcmp (function, known_function_name) != 0) {
		return FALSE;
	}
	*function_name = g_strdup (function);
	*source_file_name = g_strdup (file);

	return TRUE;
}

static NautilusLeakSymbolLookupMap *
nautilus_leak_symbol_map_load (const char *binary_path)
{
	NautilusLeakSymbolLookupMap *map;
	char *target = NULL;
	size_t storage_needed;
	int number_of_symbols;

	map = g_new0 (NautilusLeakSymbolLookupMap, 1);

	map->abfd = bfd_openr (binary_path, target);

	if (map->abfd == NULL) {
		fprintf (stderr, "%s: ", binary_path);
		bfd_perror (binary_path);
		return NULL;
	}

	if (!bfd_check_format (map->abfd, bfd_object)) {
		fprintf (stderr, "%s is not an object file\n", binary_path);
		bfd_close (map->abfd);
		return NULL;
	}

	/* Use the ".text" section.  */
	map->text_section = bfd_get_section_by_name (map->abfd, ".text");

	  /* Read the symbol table.  */
	storage_needed = bfd_get_symtab_upper_bound (map->abfd);
	if (storage_needed == 0) {
		fprintf (stderr, "no symbols\n");
		bfd_close (map->abfd);
		return NULL;
	}
	map->symbol_table = (asymbol **)g_malloc (storage_needed);
	if (map->symbol_table == NULL) {
		fprintf (stderr, "no memory allocating symbol table\n");
		bfd_close (map->abfd);
		return NULL;
	}
	number_of_symbols = bfd_canonicalize_symtab (map->abfd, map->symbol_table);
	map->path = g_strdup (binary_path);
	
	symbol_table_list = g_list_append (symbol_table_list, map);

	return map;
}

static NautilusLeakSymbolLookupMap * 
nautilus_leak_symbol_map_load_if_needed (const char *binary_path)
{
	GList *p;
	NautilusLeakSymbolLookupMap *map;
	
	for (p = symbol_table_list; p != NULL; p = p->next) {
		map = p->data;
		if (strcmp (map->path, binary_path) == 0)
			/* no need to load the symbols, already got the map */
			return map;
	}
	return nautilus_leak_symbol_map_load (binary_path);
}

void 
nautilus_leak_print_symbol_cleanup (void)
{
	/* free the cached symbol tables */
	GList *p;
	NautilusLeakSymbolLookupMap *map;
	
	for (p = symbol_table_list; p != NULL; p = p->next) {
		map = p->data;
		bfd_close (map->abfd);
		g_free (map->symbol_table);
		g_free (map->path);

		g_free (map);
	}

	g_list_free (symbol_table_list);
	symbol_table_list = NULL;
}

static gboolean
nautilus_leak_find_symbol_address (void *address, char **function_name, char **source_file_name, 
	int *line)
{
	GList *p;
	NautilusLeakSymbolLookupMap *map;
	Dl_info info;
	
	if (dladdr (address, &info) != 0) {
		/* We know the function name and the binary it lives in, now try to find
		 * the function and the offset.
		 */
		map = nautilus_leak_symbol_map_load_if_needed (info.dli_fname);
		if (map != NULL
			&& nautilus_leak_find_symbol_in_map (map, (long)address, 
				function_name, source_file_name, line, 
				(unsigned long)info.dli_fbase, info.dli_sname)) {
			return TRUE;
		}
		/* just return the function name and the library binary path */
		*function_name = g_strdup (info.dli_sname);
		*source_file_name = g_strdup (info.dli_fname);
		*line = -1;
		return TRUE;
	} else {
		/* Usually dladdr will succeed, it seems to only fail for
		 * address lookups for functions in the main binary.
		 */
		for (p = symbol_table_list; p != NULL; p = p->next) {
			map = p->data;
			if (nautilus_leak_find_symbol_in_map (map, (long)address, function_name, 
				source_file_name, (unsigned int *)line, 0, NULL))
				return TRUE;
		}
	}

	return FALSE;
}

void
nautilus_leak_print_symbol_address (const char *app_path, void *address)
{
	char *function_name;
	char *source_file_name;
	int line;

	nautilus_leak_symbol_map_load_if_needed (app_path);

	if (nautilus_leak_find_symbol_address (address, &function_name, &source_file_name, &line)) {
		if (line >= 0) {
			printf("%p %s %s:%d\n", address, function_name, source_file_name, line);
		} else {
			printf("%p %s in library %s\n", address, function_name, source_file_name);
		}
		g_free (function_name);
		g_free (source_file_name);
	} else {
		printf("%p (unknown function)\n", address);
	}
}
