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

#include "funcsummary.h"

#include <vector>
#include <algorithm>
#include <utility>
#include <iomanip>
#include <cstdio>

using namespace std;

inline bool 
FuncSummary::sort_func_percent(const FuncData &left,  const FuncData &right)
{
	return right.func_pct < left.func_pct;
}

inline bool 
FuncSummary::sort_func_calls(const FuncData &left, const FuncData &right)
{
	return right.count < left.count;
}

inline bool 
FuncSummary::sort_func_children_percent(const FuncData &left, const FuncData &right)
{
	return right.func_children_pct < left.func_children_pct;
}

FuncSummary::FuncSummary(const ProfileData &data, const SymbolTable &_syms,
			 sort_order _order, profctr_t total_time)
    : syms(_syms), order(_order)
{
	time_factor = data.frequency() / 1000;
	
	MyArcVisitor visitor(*this);
	data.VisitArcs(&visitor);
	
	for (function_map_t::iterator i = function_map.begin();
		i != function_map.end(); ++i) {
		i->second.func_pct = (double)i->second.func_time / total_time * 100;
		i->second.func_children_pct = (double)i->second.func_children_time / total_time * 100;
	}
}

FuncSummary::~FuncSummary()
{
}

const char FuncSummary::header[] =
 "Function Name                                       calls   func%  func(ms)    f+c%   f+c(ms)\n";
//wait_reply                                          19078  51.731      4074  51.737      4074


void 
FuncSummary::Output(ostream &os)
{
	typedef std::vector<FuncData> function_list_t;
	function_list_t func_list;
	
	func_list.reserve(function_map.size());
	
	for (function_map_t::const_iterator i = function_map.begin();
		i != function_map.end(); ++i)
		func_list.push_back(i->second);
	
	switch (order) {
		case sort_func:
			sort(func_list.begin(), func_list.end(), sort_func_percent);
			break;
		
		case sort_child:
			sort(func_list.begin(), func_list.end(), sort_func_children_percent);
			break;
		
		case sort_calls:
			sort(func_list.begin(), func_list.end(), sort_func_calls);
			break;
	}
	
	os << header;
	
	for (function_list_t::const_iterator i = func_list.begin();
		i != func_list.end(); ++i) {
		char buf[256];

		if (i->func_pct == 0 || i->count == 0)
			continue;
			
		snprintf(buf, sizeof(buf)/sizeof(buf[0]),
			"%-47s %9ld %#7.3f %9lld %7.3f %9lld\n", i->name.c_str(),
			(long)i->count, i->func_pct, i->func_time / time_factor,
			i->func_children_pct, i->func_children_time / time_factor);
		
		os << buf;
	}
}

FuncSummary::FuncData &FuncSummary::get_func(codeptr_t func)
{
	string name;
	syms.LookupExact(func, name);
	
	function_map_t::iterator i = function_map.find(name);
	if (i != function_map.end()) 
		return i->second;
	
	FuncData f;
	f.name = name;
	f.func_time = 0;
	f.func_children_time = 0;
	f.func_pct = 0;
	f.func_children_pct = 0;
	f.count = 0;
	
	return function_map.insert(make_pair(name, f)).first->second;
}

void FuncSummary::MyArcVisitor::visit(const ProfileData::ArcData &a)
{
	FuncData &from = outer.get_func(a.fromfunc);
	FuncData &to = outer.get_func(a.tofunc);
	
	to.count += a.count;
	
	to.func_time += a.time;
	to.func_children_time += a.time;
	
	from.func_time -= a.time;
}


FuncSummary::MyArcVisitor::~MyArcVisitor()
{
}
