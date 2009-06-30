/*
 * This file is part of UProf.
 *
 * Copyright © 2008, 2009 Robert Bragg
 *
 * UProf is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * UProf is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with UProf.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <uprof.h>

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <glib.h>
#include <errno.h>
#include <string.h>
#include <glib/gprintf.h>

#define UPROF_DEBUG     1

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

struct _UProfContext
{
  guint  ref;

  char	*name;
  GList	*counters;
  GList	*timers;

  gboolean suspended;

  GList *root_timers;
  GList *report_callbacks;

  GList *report_messages;
};

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
      unsigned long long full_value;
  } u;
} RDTSCVal;

static unsigned long long system_counter_hz;
static GList *_uprof_all_contexts;

void
uprof_init (int *argc, char ***argv)
{

}

static void
uprof_calibrate_system_counter (void)
{
  unsigned long long time0, time1 = 0;
  struct timespec delay;
  unsigned long long diff;

  if (system_counter_hz)
    return;

  /* Determine the frequency of our time source */
  DBG_PRINTF ("Determining the frequency of the system counter\n");

  while (1)
    {
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

  diff = time1 - time0;
  system_counter_hz = diff * 4;

  DBG_PRINTF ("time0: %llu\n", time0);
  DBG_PRINTF (" <sleep for 1/4 second>\n");
  DBG_PRINTF ("time1: %llu\n", time1);
  DBG_PRINTF ("Diff over 1/4 second: %llu\n", diff);
  DBG_PRINTF ("System Counter HZ: %llu\n", system_counter_hz);
}

unsigned long long
uprof_get_system_counter (void)
{
#if __i386__
  RDTSCVal rdtsc;

  /* XXX:
   * Consider that on some multi processor machines it may be necissary to set
   * processor affinity.
   *
   * For Core 2 Duo, or hyper threaded systems, as I understand it the
   * rdtsc is driven by the system bus, and so the value is synchronized
   * accross logical cores so we don't need to worry about affinity.
   *
   * Also since rdtsc is driven by the system bus, unlike the "Non-Sleep Clock
   * Ticks" counter it isn't affected by the processor going into power saving
   * states, which may happen as the processor goes idle waiting for IO.
   */

  asm ("rdtsc; movl %%edx,%0; movl %%eax,%1"
       : "=r" (rdtsc.u.split.hi), "=r" (rdtsc.u.split.low)
       : /* input */
       : "%edx", "%eax");

  return rdtsc.u.full_value;
#else
  /* XXX: implement gettimeofday() based fallback */
#error "System currently not supported by uprof"
#endif
}

unsigned long long
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

void
uprof_context_ref (UProfContext *context)
{
  context->ref++;
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

      _uprof_all_contexts = g_list_remove (_uprof_all_contexts, context);
      g_free (context);
    }
}

/* A counter or timer may be accessed from multiple places in source
 * code so we support tracking a list of locations for each uprof
 * object. Note: we don't currently seperate statistics for each
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

UProfCounterState *
find_uprof_counter_state (UProfContext *context, const char *name)
{
  return (UProfCounterState *)find_uprof_object_state (context->counters,
                                                       name);
}

void
uprof_context_add_counter (UProfContext *context, UProfCounter *counter)
{
  /* We check if we have actually seen this counter before; it might be that
   * it belongs to a dynamic shared object that has been reloaded */
  UProfCounterState *state = find_uprof_counter_state (context, counter->name);
  if (!state)
    {
      UProfObjectState *object;
      state = g_slice_alloc0 (sizeof (UProfCounterState));
      if (context->suspended)
        state->disabled = 1;
      object = (UProfObjectState *)state;
      object->name = g_strdup (counter->name);
      object->description = g_strdup (counter->description);
      object->locations = add_location (NULL,
                                        counter->filename,
                                        counter->line,
                                        counter->function);
      context->counters = g_list_prepend (context->counters, state);
    }
  counter->state = state;
}

void
uprof_context_add_timer (UProfContext *context, UProfTimer *timer)
{
  /* We check if we have actually seen this timer before; it might be that
   * it belongs to a dynamic shared object that has been reloaded */
  UProfTimerState *state =
    uprof_context_get_timer_result (context, timer->name);
  if (!state)
    {
      UProfObjectState *object;
      state = g_slice_alloc0 (sizeof (UProfTimerState));
      if (context->suspended)
        state->disabled = 1;
      object = (UProfObjectState *)state;
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
}

static GList *
find_timer_children (UProfContext *context, UProfTimerState *parent)
{
  GList *l;
  GList *children = NULL;

  for (l = context->timers; l != NULL; l = l->next)
    {
      UProfTimerState *timer = l->data;
      if (timer->parent_name &&
          strcmp (timer->parent_name, parent->object.name) == 0)
        children = g_list_prepend (children, timer);
    }
  return children;
}

/*
 * Timer parents are declared using a string to name parents; this function
 * takes the names and resolves them to actual UProfTimer structures.
 */
static void
resolve_timer_heirachy (UProfContext    *context,
                        UProfTimerState *timer,
                        UProfTimerState *parent)
{
  GList *l;
  timer->parent = parent;
  timer->children = find_timer_children (context, timer);
  for (l = timer->children; l != NULL; l = l->next)
    resolve_timer_heirachy (context, (UProfTimerState *)l->data, timer);
}

static const char *bars[] = {
    " ",
    "▏",
    "▎",
    "▍",
    "▌",
    "▋",
    "▊",
    "▉",
    "█"
};

void
uprof_print_percentage_bar (float percent)
{
  int bar_len = 3.6 * percent;
  int bar_char_len = bar_len / 8;
  int i;

  if (bar_len % 8)
    bar_char_len++;

  for (i = bar_len; i >= 8; i -= 8)
    g_printf ("%s", bars[8]);
  if (i)
    g_printf ("%s", bars[i]);

  /* NB: We can't use a field width with utf8 so we manually
   * guarantee a field with of 45 chars for any bar. */
  g_print ("%*s", (45 - bar_char_len), "");
}

static void
print_timer_and_children (UProfTimerResult              *timer,
                          UProfTimerResultPrintCallback  callback,
                          int                            indent_level,
                          gpointer                       data)
{
  char             *extra_fields;
  guint             extra_fields_width = 0;
  UProfTimerResult *root;
  GList            *children;
  GList            *l;
  float             percent;
  int               indent;

  if (callback)
    {
      extra_fields = callback (timer, &extra_fields_width, data);
      if (!extra_fields)
        return;
    }
  else
    extra_fields = g_strdup ("");

  /* percentages are reported relative to the root timer */
  root = uprof_timer_result_get_root (timer);

  indent = indent_level * 2; /* 2 spaces per indent level */

  percent = ((float)timer->total / (float)root->total) * 100.0;
  g_print (" %*s%-*s%-10.2f  %*s %7.3f%% ",
           indent,
           "",
           REPORT_COLUMN0_WIDTH - indent,
           timer->object.name,
           ((float)timer->total / uprof_get_system_counter_hz()) * 1000.0,
           extra_fields_width,
           extra_fields,
           percent);

  uprof_print_percentage_bar (percent);
  g_print ("\n");

  children = uprof_timer_result_get_children (timer);
  children = g_list_sort_with_data (children,
                                    UPROF_TIMER_SORT_TIME_INC,
                                    NULL);
  for (l = children; l != NULL; l = l->next)
    {
      UProfTimerState *child = l->data;
      if (child->count == 0)
        continue;

      print_timer_and_children (child,
                                callback,
                                indent_level + 1,
                                data);
    }
  g_list_free (children);

  g_free (extra_fields);
}

void
uprof_timer_result_print_and_children (UProfTimerResult              *timer,
                                       UProfTimerResultPrintCallback  callback,
                                       gpointer                       data)
{
  char *extra_titles = NULL;
  guint extra_titles_width = 0;

  if (callback)
    extra_titles = callback (NULL, &extra_titles_width, data);
  if (!extra_titles)
    extra_titles = g_strdup ("");

  g_print (" %-*s%s %*s %s\n",
           REPORT_COLUMN0_WIDTH,
           "Name",
           "Total msecs",
           extra_titles_width,
           extra_titles,
           "Percent");

  g_free (extra_titles);

  print_timer_and_children (timer, callback, 0, data);
}

void
uprof_context_foreach_timer (UProfContext            *context,
                             GCompareDataFunc         sort_compare_func,
                             UProfTimerResultCallback callback,
                             gpointer                 data)
{
  GList *l;
  context->timers = g_list_sort_with_data (context->timers,
                                           sort_compare_func,
                                           data);
  for (l = context->timers; l != NULL; l = l->next)
    {
      UProfTimerState *state = l->data;
      callback (state, data);
    }
}

void
uprof_context_foreach_counter (UProfContext              *context,
                               GCompareDataFunc           sort_compare_func,
                               UProfCounterResultCallback callback,
                               gpointer                   data)
{
  GList *l;
  context->counters = g_list_sort_with_data (context->counters,
                                             sort_compare_func,
                                             data);
  for (l = context->counters; l != NULL; l = l->next)
    {
      UProfCounterState *state = l->data;
      callback (state, data);
    }
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

float
uprof_timer_result_get_total_msecs (UProfTimerResult *timer)
{
  return ((float)timer->total / uprof_get_system_counter_hz()) * 1000.0;
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

void
uprof_context_add_report_callback (UProfContext *context,
                                   UProfReportCallback callback)
{
  context->report_callbacks =
    g_list_prepend (context->report_callbacks, callback);
}

void
uprof_context_remove_report_callback (UProfContext *context,
                                      UProfReportCallback callback)
{
  context->report_callbacks =
    g_list_remove (context->report_callbacks, callback);
}

static void
default_print_counter (UProfCounterResult *counter,
                       gpointer            data)
{
  gulong count = uprof_counter_result_get_count (counter);
  if (count == 0)
    return;

  g_print ("   %-*s%-5ld\n", REPORT_COLUMN0_WIDTH - 2,
           uprof_counter_result_get_name (counter),
           uprof_counter_result_get_count (counter));
}

static void
default_report_context (UProfContext *context)
{
  GList *l;

  g_print (" context: %s\n", context->name);
  g_print ("\n");
  g_print (" counters:\n");

  uprof_context_foreach_counter (context,
                                 UPROF_COUNTER_SORT_COUNT_INC,
                                 default_print_counter,
                                 NULL);

  /* FIXME: Re-implement using the public uprof API... */
  g_print ("\n");
  g_print (" timers:\n");
  for (l = context->root_timers; l != NULL; l = l->next)
    uprof_timer_result_print_and_children ((UProfTimerResult *)l->data, NULL, NULL);
}

static void
uprof_context_resolve_timer_heirachy (UProfContext *context)
{
  GList *l;

  /* Use the parent names of timers to resolve the actual parent
   * child heirachy (fill in timer .children, and .parent members) */
  for (l = context->timers; l != NULL; l = l->next)
    {
      UProfTimerState *timer = l->data;
      if (timer->parent_name == NULL)
        {
          resolve_timer_heirachy (context, timer, NULL);
          context->root_timers = g_list_prepend (context->root_timers, timer);
        }
    }
}

GList *
uprof_context_get_root_timer_results (UProfContext *context)
{
  return g_list_copy (context->root_timers);
}

void
uprof_context_output_report (UProfContext *context)
{
  GList *l;

  uprof_context_resolve_timer_heirachy (context);

  if (!context->report_callbacks)
    {
      default_report_context (context);
      return;
    }

  for (l = context->report_callbacks; l != NULL; l = l->next)
    {
      UProfReportCallback callback = l->data;
      callback (context);
    }
}

void
uprof_suspend_context (UProfContext *context)
{
  GList *l;

  if (context->suspended)
    {
      g_warning ("uprof_suspend_context calls can't be nested!\n");
      return;
    }

  /* FIXME - how can we verify that the timer hasn't already
   * been started? Currently we only expose debugging via a
   * build time macro that can only affect uprof.h */

  for (l = context->timers; l != NULL; l = l->next)
    ((UProfTimerState *)l->data)->disabled++;

  for (l = context->counters; l != NULL; l = l->next)
    ((UProfCounterState *)l->data)->disabled++;

  context->suspended = TRUE;
}

void
uprof_resume_context (UProfContext *context)
{
  GList *l;

  if (!context->suspended)
    {
      g_warning ("can't use uprof_resume_context with an un-suspended"
                 "context");
      return;
    }

  for (l = context->timers; l != NULL; l = l->next)
    ((UProfTimerState *)l->data)->disabled--;

  for (l = context->counters; l != NULL; l = l->next)
    ((UProfCounterState *)l->data)->disabled--;

  context->suspended = FALSE;
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
 * Should support heirachical timers; such that more fine grained timers
 * may be be easily enabled/disabled.
 *
 * Reports should support XML
 * Implement a simple clutter app for visualising the stats.
 */

