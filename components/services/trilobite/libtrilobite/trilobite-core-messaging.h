/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000 Eazel, Inc
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
 * Authors: J Shane Culpepper <pepper@eazel.com>
 *
 */

/*
 * libtrilobite - Useful functions shared between all services.  This
 * includes things like xml parsing, logging, error control, and others.
 *
 */

#ifndef TRILOBITE_CORE_MESSAGING_H
#define TRILOBITE_CORE_MESSAGING_H

typedef enum _MessageFormat MessageFormat;
typedef enum _MessageSeverity MessageSeverity;

enum _MessageFormat {
	FORMAT_STDOUT,   /* Print messages to screen */
	FORMAT_LOG,      /* Print messages to a logfile */
	FORMAT_WIDGET    /* Placeholder for sending messages to a widget */
};

enum _MessageSeverity {
	SEVERITY_NORMAL,   /* Normal information output */
	SEVERITY_WARNING,  /* A nonfatal error has occurred */
	SEVERITY_CRITICAL  /* A critical error has occurred */
};

void print_message (MessageFormat* format,
                    MessageSeverity* severity,
                    const char* message_buffer);

#endif /* TRILOBITE_CORE_MESSAGING_H */

