/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 *  Verifies that the signature on an RPM is good, and returns a
 *  string identifying who signed it.
 *
 *  Copyright (C) 2000 Eazel, Inc
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
 *  Authors: Robey Pointer <robey@eazel.com>
 *
 */

#ifndef _EAZEL_INSTALL_RPM_SIGNATURE_H_
#define _EAZEL_INSTALL_RPM_SIGNATURE_H_

int trilobite_check_rpm_signature (const char *filename, const char *keyring_filename, char **signer_name);

#endif	/* _EAZEL_INSTALL_RPM_SIGNATURE_H_ */
