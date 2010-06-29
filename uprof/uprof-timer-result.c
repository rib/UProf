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

#include <uprof.h>
#include <uprof-timer-result.h>

#include <glib.h>

const char *
uprof_timer_result_get_name (UProfTimerResult *timer)
{
  return timer->object.name;
}

const char *
uprof_timer_result_get_description (UProfTimerResult *timer)
{
  return timer->object.description;
}

guint64
_uprof_timer_result_get_total (UProfTimerResult *timer_state)
{
  /* If the timer is currently running then we get the in-flight total
   * without modifying the timer state. */
  if (timer_state->start != 0)
    {
      guint64 duration;
      if (G_LIKELY (!timer_state->disabled))
        duration = uprof_get_system_counter() -
          timer_state->start + timer_state->partial_duration;
      else
        duration = timer_state->partial_duration;
      return timer_state->total + duration;
    }
  else
    return timer_state->total;
}

float
uprof_timer_result_get_total_msecs (UProfTimerResult *timer_state)
{
  return ((float)_uprof_timer_result_get_total (timer_state) /
          uprof_get_system_counter_hz()) * 1000.0;
}

gulong
uprof_timer_result_get_start_count (UProfTimerResult *timer)
{
  return timer->count;
}

UProfTimerResult *
uprof_timer_result_get_parent (UProfTimerResult *timer)
{
  return timer->parent;
}

UProfTimerResult *
uprof_timer_result_get_root (UProfTimerResult *timer)
{
  while (timer->parent)
    timer = timer->parent;

  return timer;
}

void
uprof_timer_result_foreach_child (UProfTimerResult *timer,
                                  UProfTimerResultChildCallback callback,
                                  gpointer user_data)
{
  GList *l;
  for (l = timer->children; l; l = l->next)
    callback (l->data, user_data);
}

GList *
_uprof_timer_result_get_children (UProfTimerResult *timer)
{
  return g_list_copy (timer->children);
}

UProfContext *
uprof_timer_result_get_context (UProfTimerResult *timer)
{
  return timer->object.context;
}


