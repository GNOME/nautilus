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

#ifdef DEBUG_mfleming
#define DEBUG_MSG(x)	g_print x
#else
#define DEBUG_MSG(x)
#endif

#include <config.h>

#include "mozilla-events.h"
#include "gtkmozembed_internal.h"

#include "nsIContentViewer.h"
#include "nsIServiceManager.h"
#include "nsComponentManagerUtils.h"
#include "nsILocalFile.h"
#include "nsIDOMEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMNode.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIWebBrowser.h"
#include "nsIDOMNodeList.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIContentViewer.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIPresShell.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsReadableUtils.h"

static char *
get_glib_str_from_ns_str (nsAutoString string)
{

	char *cstr = ToNewCString(string);
	char *glib_str = g_strdup (cstr);

	nsMemory::Free (cstr);

	return glib_str;
}

extern "C" gboolean
mozilla_events_is_key_return (gpointer dom_event)
{
	g_return_val_if_fail (dom_event != NULL, FALSE);

	// This is the evil part of the process thanks to the fact that 
	// this thing cant be exposed in a straight C api.
	nsCOMPtr<nsIDOMKeyEvent> aKeyEvent (do_QueryInterface ((nsIDOMEvent*) dom_event));

	if (!aKeyEvent) {
		return FALSE;
	}


	PRUint32 keyCode;

	aKeyEvent->GetKeyCode (&keyCode);

#ifdef DEBUG_mfleming
	g_print ("key code is '%u' is return? %s\n", (unsigned) keyCode, 
		(keyCode == nsIDOMKeyEvent::DOM_VK_RETURN || keyCode == nsIDOMKeyEvent::DOM_VK_ENTER) ? "YES" : "NO" );
#endif

	return keyCode == nsIDOMKeyEvent::DOM_VK_RETURN || keyCode == nsIDOMKeyEvent::DOM_VK_ENTER;
}

/*
 * Return's the form's "action" URI or NULL this is not a form submission event
 */
static char *
mozilla_events_get_form_uri_from_event (gpointer dom_event)
{
	char * ret;

	g_return_val_if_fail (dom_event != NULL, FALSE);

	nsCOMPtr<nsIDOMEvent> aDOMEvent (do_QueryInterface ((nsIDOMEvent*) dom_event));

	if (!aDOMEvent) {
		return NULL;
	}
	
	nsCOMPtr<nsIDOMEventTarget> targetNode;
	
	aDOMEvent->GetTarget (getter_AddRefs (targetNode));
	
	if (!targetNode) {
		return NULL;
	}
	
	nsCOMPtr<nsIDOMHTMLInputElement> node = do_QueryInterface (targetNode);

	if (!node) {
		return NULL;
	}

	nsAutoString input_type_name;

	node->GetType (input_type_name);

	if ( ! ( input_type_name.EqualsWithConversion ("SUBMIT", PR_TRUE)
		|| input_type_name.EqualsWithConversion ("IMAGE", PR_TRUE) )
	) {
		return NULL;
	}

	nsCOMPtr<nsIDOMHTMLFormElement> form_node;

	node->GetForm (getter_AddRefs (form_node));

	if (!form_node) {
		return NULL;
	}

	nsAutoString form_action;

	form_node->GetAction (form_action);

	ret = get_glib_str_from_ns_str (form_action);

	if (ret) {
		DEBUG_MSG (("%s: form action is '%s'\n", ret));	
	}
	return ret;
}

static char *
mozilla_events_get_anchor_uri_from_event (gpointer dom_event)
{
	g_return_val_if_fail (dom_event != NULL, NULL);

	nsCOMPtr<nsIDOMEvent> aDOMEvent (do_QueryInterface ((nsIDOMEvent*) dom_event));

	if (!aDOMEvent) {
		return NULL;
	}
		
	nsCOMPtr<nsIDOMEventTarget> targetNode;
	
	aDOMEvent->GetTarget (getter_AddRefs (targetNode));
	
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
			
			// Anchor and Area tags both can have HREF's
			if (tag.EqualsWithConversion ("A", PR_TRUE)
			    || tag.EqualsWithConversion ("AREA", PR_TRUE)) {
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

						return get_glib_str_from_ns_str (nodeValue);
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

extern "C" char *
mozilla_events_get_href_for_event (gpointer dom_event)
{
	char *ret;

	/* Check for anchor and area tags first, then check for form submits */

	ret = mozilla_events_get_anchor_uri_from_event (dom_event);

	if (ret == NULL) {
		ret = mozilla_events_get_form_uri_from_event (dom_event);
	}

	return ret;
}

static nsIDocShell* 
get_primary_docshell (GtkMozEmbed *b)
{
	nsresult result;
	nsIWebBrowser *wb;
	gtk_moz_embed_get_nsIWebBrowser (b, &wb);

	nsCOMPtr<nsIDocShell> ds;

        nsCOMPtr<nsIDocShellTreeItem> browserAsItem = do_QueryInterface(wb);
	if (!browserAsItem) return NULL;

	// get the tree owner for that item
	nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
	result = browserAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
	if (!NS_SUCCEEDED (result) || ! treeOwner) return NULL;

	// get the primary content shell as an item
	nsCOMPtr<nsIDocShellTreeItem> contentItem;
	result = treeOwner->GetPrimaryContentShell(getter_AddRefs(contentItem));
	if (!NS_SUCCEEDED (result) || ! contentItem) return NULL;

	// QI that back to a docshell
	ds = do_QueryInterface(contentItem);

	return ds;
}

nsIDOMElement *
get_toplevel_doc_element (GtkMozEmbed *embed)
{

	nsCOMPtr<nsIDocShell> ds;
	nsresult rv, result;

	ds = get_primary_docshell (embed);

	if ( ! ds ) {
		return NULL;
	}

	/* get nsIPresShell */

	nsCOMPtr<nsIPresShell> presShell;
	result = ds->GetPresShell(getter_AddRefs(presShell));
	if (!NS_SUCCEEDED(result) || (!presShell)) return NULL;

	/* get nsIDocument */

	nsCOMPtr<nsIDocument> document;
	result = presShell->GetDocument(getter_AddRefs(document));
	if (!NS_SUCCEEDED(result) || (!document)) return NULL;

	/* get nsIDOMDocument */

	nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(document);
	if (!domDoc) return NULL;

	nsCOMPtr<nsIDOMElement> documentElement;

	domDoc->GetDocumentElement (getter_AddRefs (documentElement));

	return documentElement;
}

static void
navigate_to_node (GtkMozEmbed *mozilla_embed, nsIDOMNode *node)
{
	nsresult rv;
	nsCOMPtr<nsIDocShell> docShell;

	docShell = get_primary_docshell (mozilla_embed);

	if (!docShell) {
		return;
	}

	nsCOMPtr<nsIContentViewer> contentViewer;
	rv = docShell->GetContentViewer (getter_AddRefs (contentViewer));
	if (!NS_SUCCEEDED (rv) || !contentViewer) {
		return;
	}

	nsCOMPtr<nsIMarkupDocumentViewer> markupDocumentViewer = do_QueryInterface (contentViewer, &rv);
	if (!NS_SUCCEEDED (rv) || !markupDocumentViewer) {
		return;
	}
	markupDocumentViewer->ScrollToNode (node);
}

/* Why can't I use GetElementsByTagName?  I couldn't get it to work for me */
static nsIDOMNode *
get_anchor_named (nsIDOMNode *top_node, const nsAReadableString& aName)
{
	nsresult rv;
	nsAutoString src_string;
	nsCOMPtr<nsIDOMNodeList> nodes_list;
	PRUint32 i;
	nsCOMPtr<nsIDOMNode> child_node;

	top_node->GetChildNodes (getter_AddRefs(nodes_list));

	for ( i = 0 
		; nodes_list && NS_SUCCEEDED (nodes_list->Item (i, getter_AddRefs (child_node))) 
		  && child_node
		; i++
	) {
		nsAutoString currentNodeName;
//		nsAutoString currentNodeType;
		nsCOMPtr<nsIDOMHTMLAnchorElement> child_html_node = do_QueryInterface (child_node);

		PRBool has_children;

		if (child_html_node
		    && NS_SUCCEEDED (child_html_node->GetName (currentNodeName)) 
		    && currentNodeName.Equals (aName)) {
			return child_node;
		}

		rv = child_node->HasChildNodes (&has_children);

		if ( NS_SUCCEEDED (rv) && has_children) {
			nsCOMPtr<nsIDOMNode> ret;

			ret = get_anchor_named (child_node, aName);
			
			if (ret != nsnull) {
				return ret;
			}
		}
	}

	return nsnull;
}

extern "C" void
mozilla_navigate_to_anchor (GtkMozEmbed *mozilla_embed, const char *anchor)
{
	nsCOMPtr<nsIDOMNode> anchor_node;

	DEBUG_MSG (("+%s", __FUNCTION__));

	nsCOMPtr<nsIDOMNode> top_node;
	nsCOMPtr<nsIDOMElement> top_element;

	top_node = get_toplevel_doc_element (mozilla_embed);

	if (!top_node) {
		return;
	}

	top_element = do_QueryInterface (top_node);

	nsAutoString anchor_string;
	anchor_string.AssignWithConversion (anchor);

	anchor_node = get_anchor_named (top_node, anchor_string);

	if (anchor_node) {
		DEBUG_MSG (("=%s found anchor node", __FUNCTION__));
		navigate_to_node (mozilla_embed, anchor_node);
	}

	DEBUG_MSG (("-%s", __FUNCTION__));
}

#if 0
static void
debug_dom_dump (nsIDOMElement *element, int depth)
{
	char *tabs;

	nsCOMPtr<nsIDOMNodeList> dom_nodes_list;

	nsAutoString element_name_string; 
	element_name_string.AssignWithConversion ( "*" );

	tabs = g_new(char, depth+1);
	memset (tabs, (unsigned char) '\t', depth);
	tabs [depth] =0;

	element->GetElementsByTagName (element_name_string, getter_AddRefs (dom_nodes_list));

	if ( ! dom_nodes_list ) {
		g_print ("%s...no dom_nodes_list\n", tabs);
		return;
	}
	
	PRUint32 i;

	nsCOMPtr<nsIDOMNode> node;

	dom_nodes_list->GetLength (&i);

	g_print ("%s...number of elements %d\n", tabs, i);

	for ( i = 0 ; NS_SUCCEEDED (dom_nodes_list->Item (i, getter_AddRefs (node))) && node
		; i++
	) {
		nsCOMPtr<nsIDOMElement> child_element;
		nsCOMPtr<nsIDOMNamedNodeMap> attribs;
		nsCOMPtr<nsIDOMNode> href_attrib;
		nsCOMPtr<nsIDOMHTMLDocument> html_element;


		child_element = do_QueryInterface (node);

		if ( ! child_element ) {
			g_print ("%s...node is not an element (?!?!?)\n", tabs);
		}

		html_element = do_QueryInterface (node);

		if ( ! html_element ) {
			g_print ("%s...QI to HTMLElement returns NULL\n", tabs);
		}
		
		nsAutoString nodeName;
		nsAutoString anotherName;

		node->GetNodeName (anotherName);

		g_print ("%s...element node name %s\n", tabs, anotherName.ToNewCString());

		node->GetNodeValue (anotherName);

		g_print ("%s...element value %s\n", tabs, anotherName.ToNewCString());

		child_element->GetTagName (nodeName);

		g_print ("%s...element tagName %s\n", tabs, nodeName.ToNewCString());

		debug_dom_dump (child_element, depth + 1);
	}
}

/*
 * returns TRUE if the given event occurs in a SUBMIT button to a form with method=POST
 */
extern "C" gboolean
mozilla_events_is_in_form_POST_submit (gpointer dom_event)
{
	g_return_val_if_fail (dom_event != NULL, FALSE);

	nsCOMPtr<nsIDOMEvent> aDOMEvent (do_QueryInterface ((nsIDOMEvent*) dom_event));

	if (!aDOMEvent) {
		return FALSE;
	}
	
	nsCOMPtr<nsIDOMEventTarget> targetNode;
	
	aDOMEvent->GetTarget (getter_AddRefs (targetNode));
	
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


/* Why can't I use GetElementsByTagName?  I couldn't get it to work for me */
static gboolean
find_node_named_with_src (nsIDOMNode *top_node, const nsAReadableString& aName, const nsAReadableString& uri)
{
	nsresult rv;
	nsAutoString src_string;
	nsCOMPtr<nsIDOMNodeList> nodes_list;
	PRUint32 i;
	nsCOMPtr<nsIDOMNode> child_node;

	src_string.AssignWithConversion ("SRC");

	top_node->GetChildNodes (getter_AddRefs(nodes_list));

	for ( i = 0 
		; nodes_list && NS_SUCCEEDED (nodes_list->Item (i, getter_AddRefs (child_node))) 
		  && child_node
		; i++
	) {
		nsAutoString currentNodeName;
		PRBool has_children;

		if ( ! NS_SUCCEEDED (child_node->GetNodeName (currentNodeName) )) {
			continue;
		}

		if (currentNodeName.Equals (aName)) {
			nsAutoString attribute_value;
			nsCOMPtr<nsIDOMElement> element = do_QueryInterface (child_node);

			if ( element && NS_SUCCEEDED (element->GetAttribute (src_string, attribute_value)) ) {
#ifdef DEBUG_mfleming
				g_print ("...found iframe href %s\n", attribute_value.ToNewCString());
#endif

				if (attribute_value.Equals (uri)) {

#ifdef DEBUG_mfleming
					g_print ("...returning TRUE\n");
#endif
					
					return TRUE;
				}
			}
		}

		rv = child_node->HasChildNodes (&has_children);

		if ( NS_SUCCEEDED (rv) && has_children) {
			gboolean ret;

			ret = find_node_named_with_src (child_node, aName, uri);
			
			if (ret) {
				return ret;
			}
		}
	}

	return FALSE;
}

gboolean
mozilla_events_is_url_in_iframe (GtkMozEmbed *embed, const char *uri)
{
	nsCOMPtr<nsIDocShell> ds;
	gboolean ret;
	nsCOMPtr<nsIDOMElement> documentElement;
	size_t uri_len;

#ifdef DEBUG_mfleming
	g_print ("mozilla_events_is_url_in_iframe for uri '%s'\n", uri);
#endif

	documentElement = get_toplevel_doc_element (embed);
	
	if ( ! documentElement) {
		return FALSE;
	}
 
	nsAutoString iframe_string; 
	iframe_string.AssignWithConversion ("IFRAME");
	nsAutoString uri_string;
	uri_string.AssignWithConversion (uri);

	ret =  find_node_named_with_src (documentElement, iframe_string, uri_string);

#ifdef DEBUG_mfleming
	g_print (ret ? "...is in frame\n" : "is not in frame\n");
#endif
}

#endif /* 0 */

