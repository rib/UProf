/* This file is part of UProf.
 *
 * Copyright © 2008, 2009, 2010 Robert Bragg
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */

#ifndef _UPROF_COUNTER_RESULT_H_
#define _UPROF_COUNTER_RESULT_H_

#include <uprof-counter.h>

#include <glib.h>

G_BEGIN_DECLS

#ifndef UPROF_CONTEXT_TYPEDEF
typedef struct _UProfContext UProfContext;
#define UPROF_CONTEXT_TYPEDEF
#endif

typedef struct _UProfCounterState UProfCounterResult;

const char *
uprof_counter_result_get_name (UProfCounterResult *counter);

gulong
uprof_counter_result_get_count (UProfCounterResult *counter);

UProfContext *
uprof_counter_result_get_context (UProfCounterResult *counter);

G_END_DECLS

#endif /* _UPROF_COUNTER_RESULT_H_ */
