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

#ifndef _UPROF_TIMER_RESULT_H_
#define _UPROF_TIMER_RESULT_H_

#include <uprof-timer.h>

#include <glib.h>

G_BEGIN_DECLS

#ifndef UPROF_CONTEXT_TYPEDEF
typedef struct _UProfContext UProfContext;
#define UPROF_CONTEXT_TYPEDEF
#endif

typedef struct _UProfTimerState UProfTimerResult;

const char *
uprof_timer_result_get_name (UProfTimerResult *timer);

const char *
uprof_timer_result_get_description (UProfTimerResult *timer);

float
uprof_timer_result_get_total_msecs (UProfTimerResult *timer);

gulong
uprof_timer_result_get_start_count (UProfTimerResult *timer);

UProfTimerResult *
uprof_timer_result_get_parent (UProfTimerResult *timer);

UProfTimerResult *
uprof_timer_result_get_root (UProfTimerResult *timer);

/**
 * UProfTimerResultChildCallback:
 * @child: The current child being iterated.
 * @user_data: The private data passed to
 *             uprof_timer_result_foreach_child().
 *
 * A callback prototype used with uprof_timer_result_foreach_child()
 * to iterate all the children of a #UProfTimerResult.
 *
 * Since: 0.4
 */
typedef void (*UProfTimerResultChildCallback) (UProfTimerResult *child,
                                               gpointer user_data);

/**
 * uprof_timer_result_foreach_child:
 * @timer: A #UProfTimerResult whos children you want to iterate.
 * @callback: A #UProfTimerResultChildCallback called for each child
 * @user_data: Private data to pass to the callback.
 *
 * Iterates all the children of the given @timer and calls @callback
 * for each one.
 *
 * Since: 0.4
 */
void
uprof_timer_result_foreach_child (UProfTimerResult *timer,
                                  UProfTimerResultChildCallback callback,
                                  gpointer user_data);

UProfContext *
uprof_timer_result_get_context (UProfTimerResult *timer);

typedef char * (*UProfTimerResultPrintCallback) (UProfTimerResult *timer,
                                                 guint            *fields_width,
                                                 gpointer          data);

G_END_DECLS

#endif /* _UPROF_TIMER_RESULT_H_ */
