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

#include "profiledata.h"

#include <stack>
#include <stdexcept>
#include <cerrno>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

using namespace std;

namespace
{
	template <class T, class U>
	inline T round_up(T t, U u)
	{
		return (t + u - 1) & -u;
	}
	
	static void check_result_failure(void)
	{
		throw runtime_error(strerror(errno));
	}
	
	static inline void check_result(int res)
	{
		if (res == -1)
			check_result_failure();
	}
}

void ProfileData::assert_valid_arc_data_ptr(const struct ArcData *p)
    const throw()
{
    uintptr_t pu = reinterpret_cast<uintptr_t>(p);
    uintptr_t arc_table_u = reinterpret_cast<uintptr_t>(arc_table);

    assert(p == NULL
	   || (pu >= arc_table_u
	       && pu < arc_table_u + header->valid_arcs * sizeof(ArcData)));
}

inline size_t ProfileData::arc_index(const struct arc *a) const throw()
{
    assert(map_arc_table != NULL);
    return a - reinterpret_cast<const struct arc *>(arc_table_addr);
}

inline ProfileData::ArcData *ProfileData::xlate_arc(const struct arc *a) const
    throw()
{
    assert(arc_table != NULL);

    return (a == NULL) ? NULL : (arc_table + arc_index(a));
}

inline const struct thread_prof *ProfileData::get_thread(size_t i) const
    throw()
{
    assert(map_threads != NULL);
    assert(i < header->num_threads);

    const struct thread_prof *result = reinterpret_cast<const struct thread_prof *>(reinterpret_cast<uintptr_t>(map_threads) + i * header->stack_size * sizeof(struct stack_entry));

    assert(static_cast<const void *>(result) < static_cast<char *>(map) + maplen);

    return result;
}

ProfileData::ProfileData(const char *filename)
{
	map_profile(filename);
	try {
		load_header();
		load_threads();
		load_arcs();
	} catch (...) {
		unmap_profile();
		throw;
	}
	unmap_profile();
}

ProfileData::ProfileData(int file_descriptor)
{
    map_profile(file_descriptor);
    try {
		load_header();
		load_threads();
		load_arcs();
    } catch (...) {
		unmap_profile();
		throw;
    }
    unmap_profile();
}

void ProfileData::map_profile(int file_descriptor)
{
    check_result(file_descriptor);

    struct stat buf;
    check_result(fstat(file_descriptor, &buf));

    maplen = buf.st_size;
    map = mmap(NULL, maplen, PROT_READ, MAP_PRIVATE, file_descriptor, 0);

    uintptr_t mapaddr = reinterpret_cast<uintptr_t>(map);

    map_header = static_cast<profile_header *>(map);
    map_arc_table = reinterpret_cast<arc *>(mapaddr + getpagesize());

    map_threads = reinterpret_cast<thread_prof *>(mapaddr + getpagesize() + map_header->num_arcs * sizeof(struct arc));

    arc_table_addr = map_header->arc_table_addr;
}

void ProfileData::map_profile(const char *filename)
{
    int fd = open(filename, O_RDONLY | O_NOCTTY);
    check_result(fd);

    map_profile(fd);
    close(fd);
}

void ProfileData::unmap_profile(void)
{
    munmap(map, maplen);

    map = NULL;
    map_header = NULL;
    map_arc_table = NULL;
    map_threads = NULL;
    arc_table_addr = 0;
}

void ProfileData::load_header(void)
{
    header = new profile_header;
    *header = *map_header;
}

void ProfileData::load_threads(void)
{
    assert(header != NULL);
    assert(map_threads != NULL);

    // We can't really do much as the arc_table has not been created yet.
    ThreadData td;
    td.root_arc = NULL;

    threads.resize(header->num_threads, td);
}

// Both implementations do the same thing, but the second uses an
// explicit stack to avoid recursion.
#if 1
void ProfileData::update_arc_children(ArcData &a)
{
	for (ArcData *c = a.tochain; c != NULL; c = c->next_sibling) {
		assert(arc_table <= c && c < arc_table + header->valid_arcs);
		
		c->from = &a;
		c->fromfunc = a.tofunc;
		update_arc_children(*c);
	}
	
	// If we never return from a function, we never assign it a F+D
	// time. Fake one by adding up the F+D time for all its children.
	if (a.time == 0)
		for (ArcData *c = a.tochain; c != NULL; c = c->next_sibling)
			a.time += c->time;

}
#else
void ProfileData::update_arc_children(ArcData &a)
{
	typedef stack<ArcData *> arcdata_stack_t;
	arcdata_stack_t stack;
	
	stack.push(&a);
	
	while (!stack.empty()) {
		ArcData *a = stack.top();
		stack.pop();
		
		for (ArcData *c = a->tochain; c != NULL; c = c->next_sibling) {
			assert(arc_table <= c && c < arc_table + header->valid_arcs);
			
			c->from = a;
			c->fromfunc = a->tofunc;
			stack.push(c);
		}
	}
}
#endif

// Solely for debugging convenience.
static void *g_arc_table;
static void *g_arc_table_end;

void ProfileData::load_arcs(void)
{
	size_t narcs = header->valid_arcs;
	arc_table = new ArcData[narcs];
	
	g_arc_table = arc_table;
	g_arc_table_end = arc_table + narcs;
	
	for (size_t i=0; i < narcs; i++) {
		arc_table[i].from	= NULL;
		arc_table[i].tochain	= xlate_arc(map_arc_table[i].chain);
		arc_table[i].count	= map_arc_table[i].count;
		arc_table[i].time	= map_arc_table[i].func_and_children;
		arc_table[i].next_sibling = xlate_arc(map_arc_table[i].next);
		
		arc_table[i].fromfunc	= (codeptr_t)NULL;
		arc_table[i].tofunc	= map_arc_table[i].to;
	}

#ifndef NDEBUG
	for (size_t i=0; i < narcs; i++) {
		assert_valid_arc_data_ptr(arc_table[i].tochain);
		assert_valid_arc_data_ptr(arc_table[i].next_sibling);
	}
#endif

	// Now fill in the missing ArcData fields by following the child chains.
	for (size_t i=0; i < header->num_threads; i++) 
		threads[i].root_arc = xlate_arc(get_thread(i)->root_arc);


	if (header->num_threads > 0) {
		for (size_t i=0; i < header->num_threads - 1; i++)
			threads[i].root_arc->next_sibling = threads[i+1].root_arc;

		threads[header->num_threads-1].root_arc->next_sibling = NULL;
	}
	
	for (size_t i=0; i < header->num_threads; i++) {
		update_arc_children(*threads[i].root_arc);
	}
}

ProfileData::~ProfileData()
{
	delete header;
	delete[] arc_table;
}

void 
ProfileData::VisitArcs(ArcVisitor *v) const
{
	for (threads_const_iterator i = threads.begin(); i != threads.end(); i++)
		VisitArc(v, *i->root_arc);
}

// This is done in depth-first order, and is defined to always be this way.
void 
ProfileData::VisitArc(ArcVisitor *v, const ArcData &a) const
{
	assert(&a != NULL);
	assert_valid_arc_data_ptr(&a);
	
	typedef stack<const ArcData *> arcdata_stack_t;
	arcdata_stack_t stack;
	
	stack.push(&a);
	
	while (!stack.empty()) {
		const ArcData *a = stack.top();
		stack.pop();
		
		assert(a != NULL);
		assert_valid_arc_data_ptr(a);
		
		v->visit(*a);
		
		for (ArcData *c = a->tochain; c != NULL; c = c->next_sibling) {
			assert(c != NULL);
			assert_valid_arc_data_ptr(c);
			
			stack.push(c);
		}
	}
}
