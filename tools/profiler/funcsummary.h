/*
 * Cprof profiler tool
 * (C) Copyright 1999-2000 Corel Corporation   
 * 
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef FUNCSUMMARY_H_INCLUDED
#define FUNCSUMMARY_H_INCLUDED

#include <iostream>
#include <string>
#include <map>

#include "profiledata.h"
#include "symbol-table.h"

class FuncSummary
{
public:
	enum sort_order { 
		sort_func, 
		sort_child, 
		sort_calls 
	};
	
	FuncSummary(const ProfileData &data, const SymbolTable &syms,
		sort_order order, profctr_t total_time);
	~FuncSummary();
	
	void Output(std::ostream &os);

private:
    struct FuncData {
		string name;
		profctr_t func_time, func_children_time;
		double func_pct, func_children_pct;
		size_t count;
    };

	typedef std::map<std::string, FuncData> function_map_t;
	
	const SymbolTable &syms;
	function_map_t function_map;
	
	FuncData &get_func(codeptr_t);
	
	struct MyArcVisitor;
	friend struct MyArcVisitor;
	struct MyArcVisitor : ProfileData::ArcVisitor {
		explicit MyArcVisitor(FuncSummary &_outer) : outer(_outer) { }
		virtual ~MyArcVisitor();
		virtual void visit(const ProfileData::ArcData &a);
	
	private:
		FuncSummary &outer;
	};
	
	profctr_t time_factor;
	
	static const char header[];
	
	sort_order order;
	
	static inline bool sort_func_percent(const FuncData &left,
		const FuncData &right);
	static inline bool sort_func_calls(const FuncData &left,
		const FuncData &right);
	static inline bool sort_func_children_percent(const FuncData &left,
		const FuncData &right);
	
	// unimplemented
	FuncSummary(const FuncSummary &);
	void operator = (const FuncSummary &);
};

#endif
