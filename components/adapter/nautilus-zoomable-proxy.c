/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Copyright (C) 2000 Eazel, Inc.
 *                2000 SuSE GmbH.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Maciej Stachowiak <mjs@eazel.com>
 *           Martin Baulig <baulig@suse.de>
 *
 */

#include <config.h>
#include "nautilus-zoomable-proxy.h"

#undef ZOOMABLE_DEBUG

static BonoboObjectClass		*nautilus_zoomable_proxy_parent_class;

struct _NautilusZoomableProxyPrivate {
	Bonobo_Zoomable remote_zoomable;
};

static inline NautilusZoomableProxy *
nautilus_zoomable_proxy_from_servant (PortableServer_Servant servant)
{
	g_assert (NAUTILUS_IS_ZOOMABLE_PROXY (bonobo_object_from_servant (servant)));

	return NAUTILUS_ZOOMABLE_PROXY (bonobo_object_from_servant (servant));
}

static CORBA_float
impl_Nautilus_ZoomableProxy__get_level (PortableServer_Servant  servant,
					CORBA_Environment      *ev)
{
	NautilusZoomableProxy *proxy;

	proxy = nautilus_zoomable_proxy_from_servant (servant);
	return Bonobo_Zoomable__get_level (proxy->priv->remote_zoomable, ev);
}

static CORBA_float
impl_Nautilus_ZoomableProxy__get_minLevel (PortableServer_Servant  servant,
					   CORBA_Environment      *ev)
{
	NautilusZoomableProxy *proxy;

	proxy = nautilus_zoomable_proxy_from_servant (servant);
	return Bonobo_Zoomable__get_minLevel (proxy->priv->remote_zoomable, ev);
}

static CORBA_float
impl_Nautilus_ZoomableProxy__get_maxLevel (PortableServer_Servant  servant,
					   CORBA_Environment      *ev)
{
	NautilusZoomableProxy *proxy;

	proxy = nautilus_zoomable_proxy_from_servant (servant);
	return Bonobo_Zoomable__get_maxLevel (proxy->priv->remote_zoomable, ev);
}

static CORBA_boolean
impl_Nautilus_ZoomableProxy__get_hasMinLevel (PortableServer_Servant  servant,
					      CORBA_Environment      *ev)
{
	NautilusZoomableProxy *proxy;

	proxy = nautilus_zoomable_proxy_from_servant (servant);
	return Bonobo_Zoomable__get_hasMinLevel (proxy->priv->remote_zoomable, ev);
}

static CORBA_boolean
impl_Nautilus_ZoomableProxy__get_hasMaxLevel (PortableServer_Servant  servant,
					      CORBA_Environment      *ev)
{
	NautilusZoomableProxy *proxy;

	proxy = nautilus_zoomable_proxy_from_servant (servant);
	return Bonobo_Zoomable__get_hasMaxLevel (proxy->priv->remote_zoomable, ev);
}

static CORBA_boolean
impl_Nautilus_ZoomableProxy__get_isContinuous (PortableServer_Servant  servant,
					       CORBA_Environment      *ev)
{
	NautilusZoomableProxy *proxy;

	proxy = nautilus_zoomable_proxy_from_servant (servant);
	return Bonobo_Zoomable__get_isContinuous (proxy->priv->remote_zoomable, ev);
}

static Bonobo_ZoomLevelList *
impl_Nautilus_ZoomableProxy__get_preferredLevels (PortableServer_Servant  servant,
						  CORBA_Environment      *ev)
{
	NautilusZoomableProxy *proxy;

	proxy = nautilus_zoomable_proxy_from_servant (servant);
	return Bonobo_Zoomable__get_preferredLevels (proxy->priv->remote_zoomable, ev);
}

static Bonobo_ZoomLevelNameList *
impl_Nautilus_ZoomableProxy__get_preferredLevelNames (PortableServer_Servant  servant,
						      CORBA_Environment      *ev)
{
	NautilusZoomableProxy *proxy;

	proxy = nautilus_zoomable_proxy_from_servant (servant);
	return Bonobo_Zoomable__get_preferredLevelNames (proxy->priv->remote_zoomable, ev);
}

static void 
impl_Nautilus_ZoomableProxy_setLevel (PortableServer_Servant  servant,
				      const CORBA_float       zoom_level,
				      CORBA_Environment      *ev)
{
	NautilusZoomableProxy *proxy;

	proxy = nautilus_zoomable_proxy_from_servant (servant);
	Bonobo_Zoomable_setLevel (proxy->priv->remote_zoomable, zoom_level, ev);
}

static void
impl_Nautilus_ZoomableProxy_zoomIn (PortableServer_Servant  servant,
				    CORBA_Environment      *ev)
{	
	NautilusZoomableProxy *proxy;

	proxy = nautilus_zoomable_proxy_from_servant (servant);
	Bonobo_Zoomable_zoomIn (proxy->priv->remote_zoomable, ev);
}

static void
impl_Nautilus_ZoomableProxy_zoomOut (PortableServer_Servant  servant,
				     CORBA_Environment      *ev)
{	
	NautilusZoomableProxy *proxy;

	proxy = nautilus_zoomable_proxy_from_servant (servant);
	Bonobo_Zoomable_zoomOut (proxy->priv->remote_zoomable, ev);
}

static void
impl_Nautilus_ZoomableProxy_zoomFit (PortableServer_Servant  servant,
				     CORBA_Environment      *ev)
{	
	NautilusZoomableProxy *proxy;

	proxy = nautilus_zoomable_proxy_from_servant (servant);
	Bonobo_Zoomable_zoomFit (proxy->priv->remote_zoomable, ev);
}

static void
impl_Nautilus_ZoomableProxy_zoomDefault (PortableServer_Servant  servant,
					 CORBA_Environment      *ev)
{	
	NautilusZoomableProxy *proxy;

	proxy = nautilus_zoomable_proxy_from_servant (servant);
	Bonobo_Zoomable_zoomDefault (proxy->priv->remote_zoomable, ev);
}

static void
impl_Nautilus_ZoomableProxy_setFrame (PortableServer_Servant  servant,
				      Bonobo_ZoomableFrame    zoomable_frame,
				      CORBA_Environment      *ev)
{	
	NautilusZoomableProxy *proxy;

	proxy = nautilus_zoomable_proxy_from_servant (servant);
	Bonobo_Zoomable_setFrame (proxy->priv->remote_zoomable, zoomable_frame, ev);

	Bonobo_ZoomableFrame_onParametersChanged (zoomable_frame, ev);
}


static void
nautilus_zoomable_proxy_init_epv (POA_Bonobo_Zoomable__epv *epv)
{
	epv->_get_level = impl_Nautilus_ZoomableProxy__get_level;
	epv->_get_minLevel = impl_Nautilus_ZoomableProxy__get_minLevel;
	epv->_get_maxLevel = impl_Nautilus_ZoomableProxy__get_maxLevel;
	epv->_get_hasMinLevel = impl_Nautilus_ZoomableProxy__get_hasMinLevel;
	epv->_get_hasMaxLevel = impl_Nautilus_ZoomableProxy__get_hasMaxLevel;
	epv->_get_isContinuous = impl_Nautilus_ZoomableProxy__get_isContinuous;
	epv->_get_preferredLevels = impl_Nautilus_ZoomableProxy__get_preferredLevels;
	epv->_get_preferredLevelNames = impl_Nautilus_ZoomableProxy__get_preferredLevelNames;

	epv->zoomIn      = impl_Nautilus_ZoomableProxy_zoomIn;
	epv->zoomOut     = impl_Nautilus_ZoomableProxy_zoomOut;
	epv->zoomFit     = impl_Nautilus_ZoomableProxy_zoomFit;
	epv->zoomDefault = impl_Nautilus_ZoomableProxy_zoomDefault;

	epv->setLevel = impl_Nautilus_ZoomableProxy_setLevel;
	epv->setFrame = impl_Nautilus_ZoomableProxy_setFrame;
}

static void
nautilus_zoomable_proxy_destroy (BonoboObject *object)
{
	NautilusZoomableProxy *proxy;

	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_ZOOMABLE_PROXY (object));

	proxy = NAUTILUS_ZOOMABLE_PROXY (object);

	if (proxy->priv->remote_zoomable != CORBA_OBJECT_NIL)
		bonobo_object_release_unref (proxy->priv->remote_zoomable, NULL);
	proxy->priv->remote_zoomable = CORBA_OBJECT_NIL;

	BONOBO_OBJECT_CLASS (nautilus_zoomable_proxy_parent_class)->destroy (BONOBO_OBJECT (object));
}

static void
nautilus_zoomable_proxy_finalize (GObject *object)
{
	NautilusZoomableProxy *proxy;

	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_ZOOMABLE_PROXY (object));

	proxy = NAUTILUS_ZOOMABLE_PROXY (object);

	g_free (proxy->priv);
	proxy->priv = NULL;

	G_OBJECT_CLASS (nautilus_zoomable_proxy_parent_class)->finalize (object);
}

static void
nautilus_zoomable_proxy_class_init (NautilusZoomableProxyClass *klass)
{
	GObjectClass *object_class;
	BonoboObjectClass *bonobo_object_class;

	object_class = (GObjectClass *) klass;
	bonobo_object_class = (BonoboObjectClass *)klass;
	
	nautilus_zoomable_proxy_parent_class = 
		g_type_class_peek_parent (klass);
	
	bonobo_object_class->destroy = nautilus_zoomable_proxy_destroy;
	object_class->finalize = nautilus_zoomable_proxy_finalize;

	nautilus_zoomable_proxy_init_epv (&klass->epv);
}

static void
nautilus_zoomable_proxy_init (NautilusZoomableProxy *proxy)
{
	proxy->priv = g_new0 (NautilusZoomableProxyPrivate, 1);
}

/**
 * nautilus_zoomable_proxy_get_type:
 *
 * Returns: the GType for a NautilusZoomableProxy object.
 */
GType
nautilus_zoomable_proxy_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (NautilusZoomableProxyClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) nautilus_zoomable_proxy_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (NautilusZoomableProxy),
			0, /* n_preallocs */
			(GInstanceInitFunc) nautilus_zoomable_proxy_init
		};

		type = bonobo_type_unique 
			(BONOBO_TYPE_OBJECT,
			 POA_Bonobo_Zoomable__init, NULL, 
			 G_STRUCT_OFFSET (NautilusZoomableProxyClass, epv),
			 &info, "NautilusZoomableProxy");
	}

	return type;
}

BonoboObject *
nautilus_zoomable_proxy_get (Bonobo_Zoomable remote_zoomable)
{
	NautilusZoomableProxy *proxy;

	g_return_val_if_fail (remote_zoomable != CORBA_OBJECT_NIL, NULL);

	proxy = g_object_new (nautilus_zoomable_proxy_get_type (), NULL);

	proxy->priv->remote_zoomable = bonobo_object_dup_ref (remote_zoomable, NULL);

	return BONOBO_OBJECT (proxy);
}
