/* symbol-table.h - symbol lookup for a leak checking and profiling
   library

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
*/

#ifndef SYMBOL_TABLE__
#define SYMBOL_TABLE__

#include <string>
#include <stdio.h>
#include "nautilus-leak-symbol-lookup.h"

#include "profile.h"

class SymbolTable {
public:
	SymbolTable(const char *app_path)
		:	app_path(app_path)
		{}
	~SymbolTable()
		{}

	void Lookup (codeptr_t address, string &name) const
		{
			name = "";

#if 1
			char buffer[256];
			sprintf(buffer, "x/i 0x%x ", address);
			name = buffer;
#endif

			string function_name;
			get_function_at_address (app_path.c_str(), 
				(void *)address, function_name); 
			name += function_name;	
		}

	void LookupExact (codeptr_t address, string &name) const
		{ 
			// for now
			Lookup(address, name); 
		}

private:
	string app_path;
};

#endif
