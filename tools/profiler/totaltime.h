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

#ifndef TOTALTIME_H_INCLUDED
#define TOTALTIME_H_INCLUDED

#include "profiledata.h"

class TotalTime
{
public:
    explicit TotalTime(const ProfileData &data) { CountTotalTime(data); }
    virtual ~TotalTime();

    inline profctr_t total_time(void) const;

private:
    profctr_t m_total_time;

    void CountTotalTime(const ProfileData &data);

    // unimplemented
    TotalTime(const TotalTime &);
    void operator = (const TotalTime &);
};

inline profctr_t TotalTime::total_time(void) const
{
    return m_total_time;
}

#endif
