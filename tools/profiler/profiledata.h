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

#ifndef PROFILEDATA_H_INCLUDED
#define PROFILEDATA_H_INCLUDED

#include <string>
#include <vector>
#include <iterator>

#include <stdint.h>

#include <sys/types.h>
#include <unistd.h>

#include "profile.h"

class ProfileData
{
public:
	explicit ProfileData(int file_descriptor);
	explicit ProfileData(const char *filename);
	virtual ~ProfileData();
	
	// These report real-word absolute time and can be used to identify
	// the run. If you save multiple times during the same run, they will
	// have the same start_time, but different end_times.
	inline timeval start_time() const;
	inline timeval end_time() const;
	
	inline profctr_t total_time() const;
	
	inline profctr_t frequency() const;
	
	// really valid_arcs, but the outside world shouldn't know that
	// unused arcs exist at all
	inline size_t num_arcs() const;
	
	struct ArcData {
		ArcData *from, *tochain;
		codeptr_t fromfunc, tofunc;
		size_t count;
		profctr_t time;
		ArcData *next_sibling;
	};
	
	struct ThreadData {
		ArcData *root_arc;
	};
	
	struct ArcVisitor {
		virtual void visit(const ArcData &a) = 0;
	};
	
	void VisitArcs(ArcVisitor *v) const;
	
	struct const_arc_iterator;
	struct arc_iterator {
		// SGI STL does not include struct iterator :(
		typedef forward_iterator_tag iterator_category;
		typedef const ArcData value_type;
		typedef ptrdiff_t difference_type;
		typedef value_type *pointer;
		typedef value_type &reference;
		
		friend struct const_arc_iterator;
		friend class ProfileData;
		friend bool operator == (const arc_iterator &, const arc_iterator &);
		
		arc_iterator() { }
		arc_iterator(const arc_iterator &i) : p(i.p) { }
		arc_iterator &operator = (const arc_iterator &i)
		{
			p = i.p;
			return *this;
		}
		
		reference operator *() const throw() { return *p; }
		pointer operator ->() const throw() { return p; }
		
		arc_iterator &operator ++ () throw()
			{ p = p->next_sibling; return *this; }
		arc_iterator operator ++ (int) throw()
		{
			arc_iterator temp(*this);
			p = p->next_sibling;
			return temp;
		}
		
	private:
		explicit arc_iterator(ArcData *_p) : p(_p) { }
		static inline arc_iterator end() 
			{ return arc_iterator(NULL); }
		
		pointer p;
	};
	
	struct const_arc_iterator {
		// SGI STL does not include struct iterator :(
		typedef forward_iterator_tag iterator_category;
		typedef const ArcData value_type;
		typedef ptrdiff_t difference_type;
		typedef value_type *pointer;
		typedef value_type &reference;
		
		friend class ProfileData;
		friend bool operator == (const const_arc_iterator &,
		const const_arc_iterator &);
		
		const_arc_iterator() throw() { }
		const_arc_iterator(const arc_iterator &i) throw() : p(i.p) { }
		const_arc_iterator(const const_arc_iterator &i) throw() : p(i.p) { }
		
		const_arc_iterator &operator = (const arc_iterator &i) throw()
		{
			p = i.p;
			return *this;
		}
		const_arc_iterator &operator = (const const_arc_iterator &i) throw()
		{
			p = i.p;
			return *this;
		}
		
		reference operator *() const throw() { return *p; }
		pointer operator ->() const throw() { return p; }
		
		const_arc_iterator &operator ++ () throw()
			{ p = p->next_sibling; return *this; }
		const_arc_iterator operator ++ (int) throw()
		{
			const_arc_iterator temp(*this);
			p = p->next_sibling;
			return temp;
		}
		
	private:
		explicit const_arc_iterator(const ArcData *_p) throw() : p(_p) { }
		static inline const arc_iterator end() throw()
			{ return arc_iterator(NULL); }
		
		pointer p;
	};
	
	inline const_arc_iterator begin_children(const ArcData &a) const throw();
	inline const_arc_iterator end_children(const ArcData &a) const throw();
	
	inline const_arc_iterator begin_roots() const throw();
	inline const_arc_iterator end_roots() const throw();
	
private:
	void VisitArc(ArcVisitor *v, const ArcData &a) const;
	
	void assert_valid_arc_data_ptr(const ArcData *p) const throw();
	inline size_t arc_index(const struct arc *a) const throw();
	inline ArcData *xlate_arc(const struct arc *a) const throw();
	inline const struct thread_prof *get_thread(size_t i) const throw();
	
	void map_profile(const char *filename);
	void map_profile(int file_descriptor);
	void unmap_profile();
	
	void load_header();
	void load_threads();
	void load_arcs();
	void update_arc_children(ArcData &a);
	
	// These are only valid while the map is active. (i.e. during the ctor)
	void *map;							// Mmaped cmon.out.
	const profile_header *map_header;	// profiler_header in map
	const arc *map_arc_table;			// arc table in map
	const thread_prof *map_threads;		// first thread in map
	size_t maplen;						// length of entire map
	uintptr_t arc_table_addr;			// VA of arc table in profiled program
	
	profile_header *header;
	ArcData *arc_table;
	typedef std::vector<ThreadData> threads_list_t;
	typedef threads_list_t::iterator threads_iterator;
	typedef threads_list_t::const_iterator threads_const_iterator;
	threads_list_t threads;
	
	profctr_t m_total_time;
	
	// unimplemented
	ProfileData(const ProfileData &);
	void operator = (const ProfileData &);
};

inline timeval 
ProfileData::start_time() const
{
    return header->start_time;
}

inline timeval 
ProfileData::end_time() const
{
    return header->end_time;
}

inline profctr_t 
ProfileData::total_time() const
{
    return m_total_time;
}

inline profctr_t 
ProfileData::frequency() const
{
    return header->frequency;
}

inline size_t 
ProfileData::num_arcs() const
{
    return header->valid_arcs;
}

inline ProfileData::const_arc_iterator
ProfileData::begin_children(const ArcData &a) const throw()
{
    return const_arc_iterator(a.tochain);
}

inline ProfileData::const_arc_iterator
ProfileData::end_children(const ArcData &) const throw()
{
    return const_arc_iterator::end();
}

inline ProfileData::const_arc_iterator 
ProfileData::end_roots()
    const throw()
{
    return const_arc_iterator::end();
}

inline ProfileData::const_arc_iterator 
ProfileData::begin_roots()
    const throw()
{
    return (header->num_threads > 0)
	? const_arc_iterator(threads[0].root_arc)
	: end_roots();
}

inline bool 
operator == (const ProfileData::arc_iterator &left,
			 const ProfileData::arc_iterator &right)
{
    return left.p == right.p;
}

inline bool 
operator == (const ProfileData::const_arc_iterator &left,
			 const ProfileData::const_arc_iterator &right)
{
    return left.p == right.p;
}
#endif
