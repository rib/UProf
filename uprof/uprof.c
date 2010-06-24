/* This file is part of UProf.
 *
 * Copyright © 2006 OpenedHand
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

#define USE_RDTSC

#if !defined(__i386__) && !defined(__x86_64__)
#undef USE_RDTSC
#endif
//#undef USE_RDTSC

#include <uprof.h>
#include <uprof-private.h>
#include <uprof-report-private.h>
#include <uprof-service-private.h>

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <glib.h>
#include <errno.h>
#include <string.h>
#include <glib/gprintf.h>
#ifndef USE_RDTSC
#include <time.h>
#endif
#include <unistd.h>

/* #define UPROF_DEBUG     1 */

#ifdef UPROF_DEBUG
#define DBG_PRINTF(fmt, args...)              \
  do                                          \
    {                                         \
      printf ("[%s] " fmt, __FILE__, ##args); \
    }                                         \
  while (0)
#else
#define DBG_PRINTF(fmt, args...) do { } while (0)
#endif
#define REPORT_COLUMN0_WIDTH 40

#define UPROF_OBJECT_STATE(X) ((UProfObjectState *)X)

typedef struct _UProfObjectLocation
{
  char  *filename;
  long   line;
  char  *function;
} UProfObjectLocation;

typedef struct _RDTSCVal
{
  union {
      struct {
          uint32_t low;
          uint32_t hi;
      } split;
      guint64 full_value;
  } u;
} RDTSCVal;

static guint64 system_counter_hz;
static GList *_uprof_all_contexts;
#ifndef USE_RDTSC
static clockid_t clockid;
#endif

UProfContext *mainloop_context = NULL;
UProfService *service = NULL;

void
uprof_init_real (void)
{
  static gboolean initialized = FALSE;

  if (initialized)
    return;

  g_type_init ();

#ifndef USE_RDTSC
  int ret;
  struct timespec ts;

  ret = clock_getcpuclockid(0, &clockid);
  if (ret == ENOENT)
    {
      g_warning ("Using the CPU clock will be unreliable on this system if "
                 "you don't assure processor affinity");
    }
  else if (ret != 0)
    {
      const char *str = strerror (errno);
      g_warning ("Failed to get CPU clock ID: %s", str);
    }

  if (clock_gettime (CLOCK_PROCESS_CPUTIME_ID, &ts) == -1)
    {
      const char *str = strerror (errno);
      g_warning ("Failed to query CLOCK_PROCESS_CPUTIME_ID clock: %s", str);
    }
#endif

  mainloop_context = uprof_context_new ("Mainloop context");

  _uprof_report_register_dbus_type_info ();
  _uprof_service_register_dbus_type_info ();

  service = _uprof_service_new ();

  initialized = TRUE;
}

void
uprof_init (int *argc, char ***argv)
{
  uprof_init_real ();
}

UProfService *
_uprof_get_service (void)
{
  return service;
}

static gboolean
pre_parse_hook (GOptionContext  *context,
                GOptionGroup    *group,
                gpointer         data,
                GError         **error)
{
  return TRUE;
}

static gboolean
post_parse_hook (GOptionContext  *context,
                 GOptionGroup    *group,
                 gpointer         data,
                 GError         **error)
{
  uprof_init_real ();
  return TRUE;
}

static GOptionEntry uprof_args[] = {
  { NULL, },
};

GOptionGroup *
uprof_get_option_group (void)
{
  GOptionGroup *group;

  group = g_option_group_new ("uprof",
                              "UProf Options",
                              "Show UProf Options",
                              NULL,
                              NULL);

  g_option_group_set_parse_hooks (group, pre_parse_hook, post_parse_hook);
  g_option_group_add_entries (group, uprof_args);
#if 0
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
#endif

  return group;
}

static void
uprof_calibrate_system_counter (void)
{
  guint64 time0, time1 = 0;
  guint64 diff;

  if (system_counter_hz)
    return;

  /* Determine the frequency of our time source */
  DBG_PRINTF ("Determining the frequency of the system counter\n");

  while (1)
    {
      struct timespec delay;
      time0 = uprof_get_system_counter ();
      delay.tv_sec = 0;
      delay.tv_nsec = 1000000000/4;
      if (nanosleep (&delay, NULL) == -1)
        {
          if (errno == EINTR)
            continue;
          else
            g_critical ("Failed to init uprof, due to nanosleep error\n");
        }
      else
        {
          time1 = uprof_get_system_counter ();
          break;
        }
    }

  /* Note: by saving hz into a global variable, processes that involve
   * a number of components with individual uprof profiling contexts don't
   * all need to incur a callibration delay. */

  g_assert (time1 > time0);
  diff = time1 - time0;
  system_counter_hz = diff * 4;

  DBG_PRINTF ("time0: %" G_GUINT64_FORMAT "\n", time0);
  DBG_PRINTF (" <sleep for 1/4 second>\n");
  DBG_PRINTF ("time1: %" G_GUINT64_FORMAT "\n", time1);
  DBG_PRINTF ("Diff over 1/4 second: %" G_GUINT64_FORMAT "\n", diff);
  DBG_PRINTF ("System Counter HZ: %" G_GUINT64_FORMAT "\n", system_counter_hz);
}

guint64
uprof_get_system_counter (void)
{
#ifdef USE_RDTSC
  RDTSCVal rdtsc;

  /* XXX:
   * Consider that on some multi processor machines it may be necessary to set
   * processor affinity.
   *
   * For Core 2 Duo, or hyper threaded systems, as I understand it the
   * rdtsc is driven by the system bus, and so the value is synchronized
   * across logical cores so we don't need to worry about affinity.
   *
   * Also since rdtsc is driven by the system bus, unlike the "Non-Sleep Clock
   * Ticks" counter it isn't affected by the processor going into power saving
   * states, which may happen as the processor goes idle waiting for IO.
   */
  asm ("rdtsc; movl %%edx,%0; movl %%eax,%1"
       : "=r" (rdtsc.u.split.hi), "=r" (rdtsc.u.split.low)
       : /* input */
       : "%edx", "%eax");

  /* g_print ("counter = %" G_GUINT64_FORMAT "\n",
              (guint64)rdtsc.u.full_value); */
  return rdtsc.u.full_value;
#else
  struct timespec ts;
  guint64 ret;

#if 0
  /* XXX: Seems very unreliable compared to simply using rdtsc asm () as above
   */
  clock_gettime (clockid, &ts);
#else
  clock_gettime (CLOCK_MONOTONIC, &ts);
#endif

  ret = ts.tv_sec;
  ret *= 1000000000;
  ret += ts.tv_nsec;

  /* g_print ("counter = %" G_GUINT64_FORMAT "\n", (guint64)ret); */

  return ret;
#endif
}

guint64
uprof_get_system_counter_hz (void)
{
  uprof_calibrate_system_counter ();
  return system_counter_hz;
}

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
free_locations (GList *locations)
{
  GList *l;
  for (l = locations; l != NULL; l = l->next)
    {
      UProfObjectLocation *location = l->data;
      g_free (location->filename);
      g_free (location->function);
      g_slice_free (UProfObjectLocation, l->data);
    }
  g_list_free (locations);
}

void
free_object_state_members (UProfObjectState *object)
{
  g_free (object->name);
  g_free (object->description);
  free_locations (object->locations);
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
          free_object_state_members (l->data);
          g_slice_free (UProfCounterState, l->data);
        }
      g_list_free (context->counters);

      for (l = context->timers; l != NULL; l = l->next)
        {
          UProfTimerState *timer = l->data;
          free_object_state_members (l->data);
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

UProfContext *
uprof_get_mainloop_context (void)
{
  return mainloop_context;
}

/* A counter or timer may be accessed from multiple places in source
 * code so we support tracking a list of locations for each uprof
 * object. Note: we don't currently separate statistics for each
 * location, though that might be worth doing. */
GList *
add_location (GList         *locations,
              const char    *filename,
              unsigned long  line,
              const char    *function)
{
  GList *l;
  UProfObjectLocation *location;

  for (l = locations; l != NULL; l = l->next)
    {
      location = l->data;
      if (strcmp (location->filename, filename) == 0
          && location->line == line
          && strcmp (location->function, function) == 0)
        return locations;
    }
  location = g_slice_alloc (sizeof (UProfObjectLocation));
  location->filename = g_strdup (filename);
  location->line = line;
  location->function = g_strdup (function);
  return g_list_prepend (locations, location);
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
      UProfObjectState *object = (UProfObjectState *)state;
      object->locations = add_location (object->locations,
                                        counter->filename,
                                        counter->line,
                                        counter->function);
    }
  else
    {
      UProfObjectState *object;
      state = g_slice_alloc0 (sizeof (UProfCounterState));
      state->disabled = context->disabled;
      object = (UProfObjectState *)state;
      object->context = context;
      object->name = g_strdup (counter->name);
      object->description = g_strdup (counter->description);
      object->locations = add_location (NULL,
                                        counter->filename,
                                        counter->line,
                                        counter->function);
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
      UProfObjectState *object = (UProfObjectState *)state;
      object->locations = add_location (object->locations,
                                        timer->filename,
                                        timer->line,
                                        timer->function);
    }
  else
    {
      UProfObjectState *object;
      state = g_slice_alloc0 (sizeof (UProfTimerState));
      state->disabled = context->disabled;
      object = (UProfObjectState *)state;
      object->context = context;
      object->name = g_strdup (timer->name);
      object->description = g_strdup (timer->description);
      object->locations = add_location (NULL,
                                        timer->filename,
                                        timer->line,
                                        timer->function);
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

UProfContext *
uprof_find_context (const char *name)
{
  GList *l;
  for (l = _uprof_all_contexts; l != NULL; l = l->next)
    {
      UProfContext *context = l->data;
      if (strcmp (context->name, name) == 0)
        return context;
    }
  return NULL;
}

gint
_uprof_timer_compare_total_times (UProfTimerState *a,
                                  UProfTimerState *b,
                                  gpointer data)
{
  if (a->total > b->total)
    return -1;
  else if (a->total < b->total)
    return 1;
  else
    return 0;
}

gint
_uprof_timer_compare_start_count (UProfTimerState *a,
                                  UProfTimerState *b,
                                  gpointer data)
{
  if (a->count > b->count)
    return -1;
  else if (a->count < b->count)
    return 1;
  else
    return 0;
}

gint
_uprof_counter_compare_count (UProfCounterState *a,
                              UProfCounterState *b,
                              gpointer data)
{
  if (a->count > b->count)
    return -1;
  else if (a->count < b->count)
    return 1;
  else
    return 0;
}

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

GList *
uprof_timer_result_get_children (UProfTimerResult *timer)
{
  return g_list_copy (timer->children);
}

UProfContext *
uprof_timer_result_get_context (UProfTimerResult *timer)
{
  return timer->object.context;
}

const char *
uprof_counter_result_get_name (UProfCounterResult *counter)
{
  return counter->object.name;
}

gulong
uprof_counter_result_get_count (UProfCounterResult *counter)
{
  return counter->count;
}

UProfContext *
uprof_counter_result_get_context (UProfCounterResult *counter)
{
  return counter->object.context;
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

GList *
uprof_context_get_messages (UProfContext *context)
{
  GList *l = g_list_copy (context->report_messages);
  for (; l != NULL; l = l->next)
    l->data = g_strdup (l->data);
  return l;
}

/* Should be easy to add new probes to code, and ideally not need to
 * modify the profile reporting code in most cases.
 *
 * Should support simple counters
 * Should support summing the total time spent between start/stop delimiters
 * Should support hierarchical timers; such that more fine grained timers
 * may be be easily enabled/disabled.
 *
 * Reports should support XML
 * Implement a simple clutter app for visualising the stats.
 */

