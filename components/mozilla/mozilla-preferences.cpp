/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Ramiro Estrugo <ramiro@eazel.com>
 *
 */

/*
 * mozilla-preferences.cpp - A small C wrapper for poking mozilla preferences
 */

#include <config.h>

#include "mozilla-preferences.h"

#include "nsIServiceManager.h"
#include "nsIPref.h"

extern "C" gboolean
mozilla_preference_set (const char	*preference_name,
			const char	*new_value)
{
	g_return_val_if_fail (preference_name != NULL, FALSE);
	g_return_val_if_fail (new_value != NULL, FALSE);

	nsCOMPtr<nsIPref> pref = do_CreateInstance(NS_PREF_PROGID);
	
	if (pref)
	{
		nsresult rv = pref->SetCharPref (preference_name, new_value);

		return NS_SUCCEEDED (rv) ? TRUE : FALSE;
	}

	return FALSE;
}

extern "C" gboolean
mozilla_preference_set_boolean (const char	*preference_name,
				gboolean	new_boolean_value)
{
	g_return_val_if_fail (preference_name != NULL, FALSE);

	nsCOMPtr<nsIPref> pref = do_CreateInstance(NS_PREF_PROGID);
	
	if (pref)
	{
		nsresult rv = pref->SetBoolPref (preference_name,
						 new_boolean_value ? PR_TRUE : PR_FALSE);

		return NS_SUCCEEDED (rv) ? TRUE : FALSE;
	}

	return FALSE;
}

extern "C" gboolean
mozilla_preference_set_int (const char	*preference_name,
			    gint	new_int_value)
{
	g_return_val_if_fail (preference_name != NULL, FALSE);

	nsCOMPtr<nsIPref> pref = do_CreateInstance(NS_PREF_PROGID);
	
	if (pref)
	{
		nsresult rv = pref->SetIntPref (preference_name, new_int_value);

		return NS_SUCCEEDED (rv) ? TRUE : FALSE;
	}

	return FALSE;
}

// 		pref->SetBoolPref("nglayout.widget.gfxscrollbars", PR_FALSE);
// 		pref->SetBoolPref("security.checkloaduri", PR_FALSE);
