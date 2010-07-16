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
#include <uprof-private.h>
#include <uprof-context.h>
#include <uprof-context-private.h>
#include <uprof-object-state-private.h>
#include <uprof-counter-result.h>
#include <uprof-timer-result.h>
#include <uprof-timer-result-private.h>
#include <uprof-counter-result-private.h>

#include <glib.h>

#include <string.h>

UProfContext *
uprof_context_new (const char *name)
{
  UProfContext *context = g_new0 (UProfContext, 1);
  context->ref = 1;

  context->name = g_strdup (name);

  _uprof_all_contexts = g_list_prepend (_uprof_all_contexts, context);
  return context;
}

UProfContext *
uprof_context_ref (UProfContext *context)
{
  context->ref++;
  return context;
}

void
uprof_context_unref (UProfContext *context)
{
  context->ref--;
  if (!context->ref)
    {
      GList *l;
      g_free (context->name);

      for (l = context->counters; l != NULL; l = l->next)
        {
          _uprof_object_state_dispose (l->data);
          g_slice_free (UProfCounterState, l->data);
        }
      g_list_free (context->counters);

      for (l = context->timers; l != NULL; l = l->next)
        {
          UProfTimerState *timer = l->data;
          _uprof_object_state_dispose (l->data);
          if (timer->parent && timer->parent_name)
            g_free (timer->parent_name);
          g_slice_free (UProfTimerState, l->data);
        }
      g_list_free (context->timers);

      for (l = context->report_messages; l != NULL; l = l->next)
        g_free (l->data);
      g_list_free (context->report_messages);

      for (l = context->links; l != NULL; l = l->next)
        uprof_context_unref (l->data);
      g_list_free (context->links);

      _uprof_all_contexts = g_list_remove (_uprof_all_contexts, context);
      g_free (context);
    }
}

const char *
uprof_context_get_name (UProfContext *context)
{
  return context->name;
}

UProfObjectState *
find_uprof_object_state (GList *objects, const char *name)
{
  GList *l;
  for (l = objects; l != NULL; l = l->next)
    if (strcmp (((UProfObjectState *)l->data)->name, name) == 0)
      return l->data;
  return NULL;
}

UProfTimerResult *
uprof_context_get_timer_result (UProfContext *context, const char *name)
{
  return (UProfTimerResult *)find_uprof_object_state (context->timers, name);
}

UProfCounterResult *
uprof_context_get_counter_result (UProfContext *context, const char *name)
{
  return (UProfCounterResult *)find_uprof_object_state (context->counters,
                                                        name);
}

typedef struct
{
  GList *seen_contexts;
  UProfContextCallback callback;
  gpointer user_data;
} ForContextAndLinksState;

static void
for_self_and_links_recursive (UProfContext *context,
                              ForContextAndLinksState *state)
{
  GList *l;

  if (g_list_find (state->seen_contexts, context))
    return;

  state->callback (context, state->user_data);

  state->seen_contexts = g_list_prepend (state->seen_contexts, context);

  for (l = context->links; l; l = l->next)
    for_self_and_links_recursive (l->data, state);
}

/* This recursively traverses all the contexts linked to the given
 * context ignoring duplicates. */
void
_uprof_context_for_self_and_links_recursive (UProfContext *context,
                                             UProfContextCallback callback,
                                             gpointer user_data)
{
  ForContextAndLinksState state;

  state.seen_contexts = NULL;
  state.callback = callback;
  state.user_data = user_data;

  for_self_and_links_recursive (context, &state);

  g_list_free (state.seen_contexts);
}

static void
dirty_resolved_state_cb (UProfContext *context,
                         gpointer user_data)
{
  context->resolved = FALSE;
}

/* If we add timers or counters or link or unlink a
 * context, then we need to scrap any resolved hierarchies
 * between timers etc. */
static void
_uprof_context_dirty_resolved_state (UProfContext *context)
{
  _uprof_context_for_self_and_links_recursive (context,
                                               dirty_resolved_state_cb,
                                               NULL);
}

void
uprof_context_add_counter (UProfContext *context, UProfCounter *counter)
{
  /* We check if we have actually seen this counter before; it might be that
   * it belongs to a dynamic shared object that has been reloaded */
  UProfCounterState *state =
    uprof_context_get_counter_result (context, counter->name);

  /* If we have seen this counter before see if it is being added from a
   * new location and track that location if so.
   */
  if (G_UNLIKELY (state))
    {
      _uprof_object_state_add_location (UPROF_OBJECT_STATE (state),
                                        counter->filename,
                                        counter->line,
                                        counter->function);
    }
  else
    {
      state = g_slice_alloc0 (sizeof (UProfCounterState));
      _uprof_object_state_init (UPROF_OBJECT_STATE (state),
                                context,
                                counter->name,
                                counter->description);
      _uprof_object_state_add_location (UPROF_OBJECT_STATE (state),
                                        counter->filename,
                                        counter->line,
                                        counter->function);
      state->disabled = context->disabled;
      context->counters = g_list_prepend (context->counters, state);
    }
  counter->state = state;
  _uprof_context_dirty_resolved_state (context);
}

void
uprof_context_add_timer (UProfContext *context, UProfTimer *timer)
{
  /* We check if we have actually seen this timer before; it might be that
   * it belongs to a dynamic shared object that has been reloaded */
  UProfTimerState *state =
    uprof_context_get_timer_result (context, timer->name);

  /* If we have seen this timer before see if it is being added from a
   * new location and track that location if so.
   */
  if (G_UNLIKELY (state))
    {
      _uprof_object_state_add_location (UPROF_OBJECT_STATE (state),
                                        timer->filename,
                                        timer->line,
                                        timer->function);
    }
  else
    {
      state = g_slice_alloc0 (sizeof (UProfTimerState));
      _uprof_object_state_init (UPROF_OBJECT_STATE (state),
                                context,
                                timer->name,
                                timer->description);
      _uprof_object_state_add_location (UPROF_OBJECT_STATE (state),
                                        timer->filename,
                                        timer->line,
                                        timer->function);

      state->disabled = context->disabled;
      if (timer->parent_name)
        state->parent_name = g_strdup (timer->parent_name);
      context->timers = g_list_prepend (context->timers, state);
    }
  timer->state = state;
  _uprof_context_dirty_resolved_state (context);
}

void
uprof_context_link (UProfContext *context, UProfContext *other)
{
  if (!g_list_find (context->links, other))
    {
      context->links = g_list_prepend (context->links,
                                       uprof_context_ref (other));

      _uprof_context_dirty_resolved_state (context);
    }
}

void
uprof_context_unlink (UProfContext *context, UProfContext *other)
{
  GList *l = g_list_find (context->links, other);
  if (l)
    {
      uprof_context_unref (other);
      context->links = g_list_delete_link (context->links, l);
      _uprof_context_dirty_resolved_state (context);
    }
}

static void
copy_timers_list_cb (UProfContext *context, gpointer user_data)
{
  GList **all_timers = user_data;
  GList *context_timers;
#ifdef DEBUG_TIMER_HEIRACHY
  GList *l;
#endif

  context_timers = g_list_copy (context->timers);
  *all_timers = g_list_concat (*all_timers, context_timers);

#ifdef DEBUG_TIMER_HEIRACHY
  g_print (" all %s timers:\n", context->name);
  for (l = context->timers; l != NULL; l = l->next)
    g_print ("  timer->name: %s\n", ((UProfObjectState *)l->data)->name);
#endif

  return;
}

void
uprof_context_foreach_timer (UProfContext            *context,
                             GCompareDataFunc         sort_compare_func,
                             UProfTimerResultCallback callback,
                             gpointer                 data)
{
  GList *l;
  GList *timers;
  GList *all_timers = NULL;

  g_return_if_fail (context != NULL);
  g_return_if_fail (callback != NULL);

#ifdef DEBUG_TIMER_HEIRACHY
  g_print ("uprof_context_foreach_timer (context = %s):\n",
           context->name);
#endif

  /* If the context has been linked with other contexts, then we want
   * a flat list of timers we can sort...
   *
   * XXX: If for example there are 3 contexts A, B and C where A and B
   * are linked and then C is linked to A and B then we are careful
   * not to duplicate the timers of C twice.
   */
  /* XXX: may want a dirty flag mechanism to avoid repeating this
   * too often! */
  if (context->links)
    {
      _uprof_context_for_self_and_links_recursive (context,
                                                   copy_timers_list_cb,
                                                   &all_timers);
      timers = all_timers;
    }
  else
    timers = context->timers;

#ifdef DEBUG_TIMER_HEIRACHY
  g_print (" all combined timers:\n");
  for (l = timers; l != NULL; l = l->next)
    g_print ("  timer->name: %s\n", ((UProfObjectState *)l->data)->name);
#endif

  if (sort_compare_func)
    timers = g_list_sort_with_data (timers, sort_compare_func, data);
  for (l = timers; l != NULL; l = l->next)
    callback (l->data, data);

  g_list_free (all_timers);
}

static void
copy_counters_list_cb (UProfContext *context, gpointer user_data)
{
  GList **all_counters = user_data;
  GList *context_counters = g_list_copy (context->counters);

  *all_counters = g_list_concat (*all_counters, context_counters);

  return;
}

void
uprof_context_foreach_counter (UProfContext              *context,
                               GCompareDataFunc           sort_compare_func,
                               UProfCounterResultCallback callback,
                               gpointer                   data)
{
  GList *l;
  GList *counters;
  GList *all_counters = NULL;

  g_return_if_fail (context != NULL);
  g_return_if_fail (callback != NULL);

  /* If the context has been linked with other contexts, then we want
   * a flat list of counters we can sort... */
  /* XXX: may want a dirty flag mechanism to avoid repeating this
   * too often! */
  if (context->links)
    {
      _uprof_context_for_self_and_links_recursive (context,
                                                   copy_counters_list_cb,
                                                   &all_counters);
      counters = all_counters;
    }
  else
    counters = context->counters;

  if (sort_compare_func)
    counters = g_list_sort_with_data (counters, sort_compare_func, data);
  for (l = counters; l != NULL; l = l->next)
    callback (l->data, data);

  g_list_free (all_counters);
}

GList *
uprof_context_get_root_timer_results (UProfContext *context)
{
  return g_list_copy (context->root_timers);
}

static void
_uprof_suspend_single_context (UProfContext *context)
{
  GList *l;

  context->disabled++;

  for (l = context->timers; l != NULL; l = l->next)
    {
      UProfTimerState *timer = l->data;

      timer->disabled++;
      if (timer->start && timer->disabled == 1)
        {
          timer->partial_duration +=
            uprof_get_system_counter () - timer->start;
        }
    }

  for (l = context->counters; l != NULL; l = l->next)
    ((UProfCounterState *)l->data)->disabled++;
}

void
uprof_context_suspend (UProfContext *context)
{
  _uprof_context_for_self_and_links_recursive (
                           context,
                           (UProfContextCallback)_uprof_suspend_single_context,
                           NULL);
}

static void
_uprof_resume_single_context (UProfContext *context)
{
  GList *l;

  context->disabled--;

  for (l = context->timers; l != NULL; l = l->next)
    {
      UProfTimerState *timer = l->data;
      timer->disabled--;
      /* Handle resuming of timers... */
      /* NB: any accumulated ->partial_duration will be added to the total
       * when the timer is stopped. */
      if (timer->start && timer->disabled == 0)
        timer->start = uprof_get_system_counter ();
    }

  for (l = context->counters; l != NULL; l = l->next)
    ((UProfCounterState *)l->data)->disabled--;
}

void
uprof_context_resume (UProfContext *context)
{
  _uprof_context_for_self_and_links_recursive (
                           context,
                           (UProfContextCallback)_uprof_resume_single_context,
                           NULL);
}

void
uprof_context_add_report_message (UProfContext *context,
                                  const char *format, ...)
{
  va_list ap;
  char *report_message;

  va_start (ap, format);
  report_message = g_strdup_vprintf (format, ap);
  va_end (ap);

  context->report_messages = g_list_prepend (context->report_messages,
                                             report_message);
}

void
uprof_context_foreach_message (UProfContext *context,
                               UProfMessageCallback callback,
                               gpointer user_data)
{
  GList *l;
  for (l = context->report_messages; l; l = l->next)
    callback (l->data, user_data);
}

void
_uprof_context_reset (UProfContext *context)
{
  GList *l;

  for (l = context->timers; l; l = l->next)
    _uprof_timer_result_reset (l->data);
  for (l = context->counters; l; l = l->next)
    _uprof_counter_result_reset (l->data);
}

typedef struct
{
  int id;
  UProfContextTraceMessageCallback callback;
  void *user_data;
} UProfContextTraceMessageFunc;

int
_uprof_context_add_trace_message_callback (
                                     UProfContext *context,
                                     UProfContextTraceMessageCallback callback,
                                     void *user_data)
{
  UProfContextTraceMessageFunc *func =
    g_slice_new (UProfContextTraceMessageFunc);

  func->id = ++context->next_trace_message_callbacks_id;
  func->callback = callback;
  func->user_data = user_data;

  context->trace_message_callbacks =
    g_list_prepend (context->trace_message_callbacks, func);

  return func->id;
}

void
_uprof_context_remove_trace_message_callback (UProfContext *context,
                                              int id)
{
  GList *l;

  for (l = context->trace_message_callbacks; l; l = l->next)
    {
      UProfContextTraceMessageFunc *func = l->data;
      if (func->id == id)
        {
          g_slice_free (UProfContextTraceMessageFunc, func);
          context->trace_message_callbacks =
            g_list_delete_link (context->trace_message_callbacks, l);
          return;
        }
    }
}

void
uprof_context_vtrace_message (UProfContext *context,
                              const char *format,
                              va_list ap)
{
  if (context->trace_message_callbacks)
    {
      char *message;
      GList *l;

      message = g_strdup_vprintf (format, ap);
      for (l = context->trace_message_callbacks; l; l = l->next)
        {
          UProfContextTraceMessageFunc *func = l->data;
          func->callback (context, message, func->user_data);
        }
      g_free (message);
    }
}

void
uprof_context_trace_message (UProfContext *context,
                             const char *format,
                             ...)
{
  va_list ap;

  va_start (ap, format);
  uprof_context_vtrace_message (context, format, ap);
  va_end (ap);
}



