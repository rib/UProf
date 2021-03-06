/* This file is part of UProf.
 *
 * Copyright © 2010 Robert Bragg
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

#ifndef _UPROF_TIMER_RESULT_PRIVATE_H_
#define _UPROF_TIMER_RESULT_PRIVATE_H_

guint64
_uprof_timer_result_get_total (UProfTimerResult *timer_state);

GList *
_uprof_timer_result_get_children (UProfTimerResult *timer);

void
_uprof_timer_result_reset (UProfTimerResult *timer);

#endif /* _UPROF_TIMER_RESULT_PRIVATE_H_ */

