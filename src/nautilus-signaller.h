
/*
 * SPDX-FileCopyrightText: 1999, 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: John Sullivan <sullivan@eazel.com>
 */

/* nautilus-signaller.h: Class to manage nautilus-wide signals that don't
 * correspond to any particular object.
 */

#pragma once

#include <glib-object.h>

/* NautilusSignaller is a class that manages signals between
   disconnected Nautilus code. Nautilus objects connect to these signals
   so that other objects can cause them to be emitted later, without
   the connecting and emit-causing objects needing to know about each
   other. It seems a shame to have to invent a subclass and a special
   object just for this purpose. Perhaps there's a better way to do 
   this kind of thing.
*/

/* Get the one and only NautilusSignaller to connect with or emit signals for */
GObject *nautilus_signaller_get_current (void);
