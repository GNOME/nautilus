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

#include <config.h>
#include "nautilus-signaller.h"


typedef GObject NautilusSignaller;
typedef GObjectClass NautilusSignallerClass;

enum
{
    HISTORY_LIST_CHANGED,
    POPUP_MENU_CHANGED,
    MIME_DATA_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static GType nautilus_signaller_get_type (void);

G_DEFINE_TYPE (NautilusSignaller, nautilus_signaller, G_TYPE_OBJECT);

GObject *
nautilus_signaller_get_current (void)
{
    static GObject *global_signaller = NULL;

    if (global_signaller == NULL)
    {
        global_signaller = g_object_new (nautilus_signaller_get_type (), NULL);
    }

    return global_signaller;
}

static void
nautilus_signaller_init (NautilusSignaller *signaller)
{
}

static void
nautilus_signaller_class_init (NautilusSignallerClass *class)
{
    signals[HISTORY_LIST_CHANGED] =
        g_signal_new ("history-list-changed",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
    signals[POPUP_MENU_CHANGED] =
        g_signal_new ("popup-menu-changed",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
    signals[MIME_DATA_CHANGED] =
        g_signal_new ("mime-data-changed",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
}
