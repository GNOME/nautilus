/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Mike Fleming <mfleming@eazel.com>
 *
 */

#ifndef EAZEL_INSTALLER_PROXY_H
#define EAZEL_INSTALLER_PROXY_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

gboolean attempt_http_proxy_autoconfigure (const char *homedir);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* EAZEL_INSTALLER_PROXY_H */
