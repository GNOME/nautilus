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
 * mozilla-event.cpp - A small C wrapper for grokking mozilla dom events
 */

#define nopeDEBUG_ramiro 1
#define nopeDEBUG_mfleming 1

#include <config.h>

#include "mozilla-events.h"

#include "nsIServiceManager.h"
#include "nsComponentManagerUtils.h"
#include "nsILocalFile.h"
#include "nsIDOMEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMNode.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMNamedNodeMap.h"

extern "C" char *
mozilla_events_get_href_for_mouse_event (gpointer mouse_event)
{
	g_return_val_if_fail (mouse_event != NULL, NULL);

	// This is the evil part of the process thanks to the fact that 
	// this thing cant be exposed in a straight C api.
	nsIDOMEvent* aMouseEvent = (nsIDOMEvent*) mouse_event;
	
	nsCOMPtr<nsIDOMMouseEvent> mouseEvent (do_QueryInterface (aMouseEvent));

	if (!mouseEvent) {
		return NULL;
	}
	
	nsCOMPtr<nsIDOMEventTarget> targetNode;
	
	aMouseEvent->GetTarget (getter_AddRefs (targetNode));
	
	if (!targetNode) {
		return NULL;
	}
	
	nsCOMPtr<nsIDOMNode> node = do_QueryInterface (targetNode);

	if (!node) {
		return NULL;
	}

	nsCOMPtr<nsIDOMHTMLElement> element = do_QueryInterface (node);

	// Travese the dom tree from the current node until an A (anchor)
	// tag is found or we reach to top node (the one without a parent)
	do {
		element = do_QueryInterface (node);

		if (element) {
			nsAutoString tag;
			element->GetTagName (tag);
			
			// Look for anchors (A)
			if (tag.EqualsWithConversion ("A", PR_TRUE))
			{
				// Test if the element has an associated link
				nsCOMPtr<nsIDOMNamedNodeMap> attributes;

				node->GetAttributes(getter_AddRefs (attributes));

				if (attributes) {
					nsAutoString href; 

					href.AssignWithConversion ("href");
					
					nsCOMPtr<nsIDOMNode> hrefNode;
					attributes->GetNamedItem (href, getter_AddRefs (hrefNode));

					if (hrefNode) {
						nsAutoString nodeValue;
						
						hrefNode->GetNodeValue (nodeValue);
						
						// This nonsense is needed to make sure the right deallocator 
						// is called on the Nautilus universe
						char *cstr = nodeValue.ToNewCString();
						char *lifeSucks = g_strdup (cstr);
		
						nsMemory::Free (cstr);
						
						return lifeSucks;
					}
				}
			}
		}
		
		nsCOMPtr<nsIDOMNode> parentNode;

		node->GetParentNode (getter_AddRefs (parentNode));
		
		// Test if we're at the top of the document
		if (!parentNode) {
			node = nsnull;
			break;
		}
		
		node = parentNode;

	} while (node);
	
	return NULL;
}


/*
 * returns TRUE if the given event occurs in a SUBMIT button to a form with method=POST
 */
/* FIXME this doesn't handle form submissions with a BUTTON or by hitting <return> */
extern "C" gboolean
mozilla_events_is_in_form_POST_submit (gpointer mouse_event)
{
	g_return_val_if_fail (mouse_event != NULL, FALSE);

	nsIDOMEvent* aMouseEvent = (nsIDOMEvent*) mouse_event;
	
	nsCOMPtr<nsIDOMMouseEvent> mouseEvent (do_QueryInterface (aMouseEvent));

	if (!mouseEvent) {
		return FALSE;
	}
	
	nsCOMPtr<nsIDOMEventTarget> targetNode;
	
	aMouseEvent->GetTarget (getter_AddRefs (targetNode));
	
	if (!targetNode) {
		return FALSE;
	}
	
	nsCOMPtr<nsIDOMHTMLInputElement> node = do_QueryInterface (targetNode);

	if (!node) {
		return FALSE;
	}

	nsAutoString input_type_name;

	node->GetType (input_type_name);

#ifdef DEBUG_mfleming
	char *cstr = input_type_name.ToNewCString();
	g_print ("input node of type '%s'\n", cstr);
	nsMemory::Free (cstr);
#endif

	if ( ! ( input_type_name.EqualsWithConversion ("SUBMIT", PR_TRUE)
		|| input_type_name.EqualsWithConversion ("IMAGE", PR_TRUE) )
	) {
		return FALSE;
	}

	nsCOMPtr<nsIDOMHTMLFormElement> form_node;

	node->GetForm (getter_AddRefs (form_node));

	if (!form_node) {
		return FALSE;
	}

	nsAutoString form_method;

	form_node->GetMethod (form_method);

	if (form_method.EqualsWithConversion ("POST", PR_TRUE)) {
		return TRUE;
	}

	return FALSE;
}
