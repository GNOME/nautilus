/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-gtk-macros.h: Macros to reduce boilerplate when using GTK.
 
   Copyright (C) 1999 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Darin Adler <darin@eazel.com>
*/

#ifndef NAUTILUS_GTK_MACROS_H
#define NAUTILUS_GTK_MACROS_H

/* Define a get_type function for a GTK class.
   Since this is boilerplate, it's better not to repeat it over and over again.
   Called like this:

       NAUTILUS_DEFINE_GET_TYPE_FUNCTION(NautilusBookmark, nautilus_bookmark, GTK_TYPE_OBJECT)

   The parent_class_type parameter is guaranteed to be evaluated only once
   so it can be an expression, even an expression that contains a function call.
*/

#define NAUTILUS_DEFINE_GET_TYPE_FUNCTION(class_name, class_name_in_function_format, parent_class_type) \
\
GtkType \
class_name_in_function_format##_get_type (void) \
{ \
	static GtkType type; \
        \
	if (type == 0) { \
		static GtkTypeInfo info = { \
			#class_name, \
			sizeof (class_name), \
			sizeof (class_name##Class), \
			(GtkClassInitFunc)class_name_in_function_format##_initialize_class, \
			(GtkObjectInitFunc)class_name_in_function_format##_initialize, \
			NULL, \
			NULL, \
			NULL \
		}; \
		\
		type = gtk_type_unique ((parent_class_type), &info); \
	} \
        \
	return type; \
}

/* Call a parent class version of a signal.
   Nice because it documents what it's doing and there is less chance for
   a typo. Depends on the parent class pointer having the conventional
   name "parent_class".
*/

#define NAUTILUS_CALL_PARENT_CLASS(parent_class_cast_macro, signal, parameters) \
\
G_STMT_START { \
	if (parent_class_cast_macro (parent_class)->signal != NULL) \
		(* parent_class_cast_macro (parent_class)->signal) parameters; \
} G_STMT_END


#ifndef G_DISABLE_ASSERT

/* Define a signal that is not implemented by this class but must be 
 * implemented by subclasses. This macro should be used inside the
 * class initialization function. The companion macro NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL
 * must be used earlier in the file. Called like this:
 * 
 * NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (FM_DIRECTORY_VIEW_CLASS,
 *					 class,
 *					 fm_directory_view,
 *					 clear); 
 */
#define NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL(class_cast_macro, class_pointer, class_name_in_function_format, signal) \
\
* (void (**)(void)) &class_cast_macro (class_pointer)->signal = class_name_in_function_format##_unimplemented_##signal;

/* Provide a debug-only implementation of a signal that must be implemented
 * by subclasses. The debug-only implementation fires a warning if it is called.
 * This macro should be placed as if it were a function, earlier in the file
 * than the class initialization function. Called like this:
 * 
 * NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (fm_directory_view, clear);
 */
#define NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL(class_name_in_function_format, signal) \
\
static void \
class_name_in_function_format##_unimplemented_##signal (void) \
{ \
	g_warning ("Failed to override signal %s->%s", #class_name_in_function_format, #signal); \
}

#else

#define NAUTILUS_DEFINE_MUST_OVERRIDE_SIGNAL(class_cast_macro, class_pointer, class_name_in_function_format, signal)
#define NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL(class_name_in_function_format, signal)

#endif /* G_DISABLE_ASSERT */



#endif /* NAUTILUS_GTK_MACROS_H */
