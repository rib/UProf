/* This file is part of UProf.
 *
 * Copyright © 2008, 2009 Robert Bragg
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

#ifndef __i386__
#undef USE_RDTSC
#endif
//#undef USE_RDTSC

#include <uprof.h>

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

typedef struct _UProfTimerAttribute
{
  char *name;
  UProfTimersAttributeReportCallback callback;
  void *user_data;
} UProfTimerAttribute;

typedef struct _UProfCounterAttribute
{
  char *name;
  UProfCountersAttributeReportCallback callback;
  void *user_data;
} UProfCounterAttribute;

struct _UProfContext
{
  guint  ref;

  char	*name;

  GList *links;

  GList	*counters;
  GList	*timers;

  gboolean suspended;

  gboolean resolved;
  GList *root_timers;

  GList *report_callbacks;
  GList *report_messages;

  GList *timer_attributes;
  GList *counter_attributes;
};

typedef struct _UProfObjectLocation
{
  char  *filename;
  long   line;
  char  *function;
} UProfObjectLocation;

typedef struct _UProfReport
{
  int max_timer_name_size;
} UProfReport;

typedef struct _RDTSCVal
{
  union {
      struct {
          uint32_t low;
          uint32_t hi;
      } split;
      uint64_t full_value;
  } u;
} RDTSCVal;

static uint64_t system_counter_hz;
static GList *_uprof_all_contexts;
#ifndef USE_RDTSC
static clockid_t clockid;
#endif

void
uprof_init (int *argc, char ***argv)
{
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
}

static void
uprof_calibrate_system_counter (void)
{
  uint64_t time0, time1 = 0;
  uint64_t diff;

  if (system_counter_hz)
    return;

  /* Determine the frequency of our time source */
  DBG_PRINTF ("Determining the frequency of the system counter\n");

  while (1)
    {
      struct timespec delay;
      time0 = uprof_get_system_counter ();
      delay.tv_nsec = 0;
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

  DBG_PRINTF ("time0: %llu\n", time0);
  DBG_PRINTF (" <sleep for 1/4 second>\n");
  DBG_PRINTF ("time1: %llu\n", time1);
  DBG_PRINTF ("Diff over 1/4 second: %llu\n", diff);
  DBG_PRINTF ("System Counter HZ: %llu\n", system_counter_hz);
}

uint64_t
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

  /* g_print ("counter = %llu\n", (uint64_t)rdtsc.u.full_value); */
  return rdtsc.u.full_value;
#else
  struct timespec ts;
  uint64_t ret;

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

  /* g_print ("counter = %llu\n", (uint64_t)ret); */

  return ret;
#endif
}

uint64_t
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
}

/* If we link or unlink a context, then we need to scrap any
 * resolved heirachies between timers etc. */
static void
_uprof_context_dirty_resolved_state (UProfContext *context)
{
  GList *l;

  context->resolved = FALSE;

  for (l = context->links; l != NULL; l = l->next)
    {
      ((UProfContext *)l->data)->resolved = FALSE;
    }
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

void
uprof_context_add_timers_attribute (UProfContext *context,
                                    const char *attribute_name,
                                    UProfTimersAttributeReportCallback callback,
                                    void *user_data)
{
  UProfTimerAttribute *attribute = g_slice_new (UProfTimerAttribute);
  attribute->name = g_strdup (attribute_name);
  attribute->callback = callback;
  attribute->user_data = user_data;
  context->timer_attributes =
    g_list_prepend (context->timer_attributes, attribute);
}

void
uprof_context_remove_timers_attribute (UProfContext *context,
                                       const char *attribute_name)
{
  GList *l;
  for (l = context->timer_attributes; l; l = l->next)
    {
      UProfTimerAttribute *attribute = l->data;
      if (strcmp (attribute->name, attribute_name) == 0)
        {
          g_free (attribute->name);
          g_slice_free (UProfTimerAttribute, attribute);
          context->timer_attributes =
            g_list_delete_link (context->timer_attributes, l);
          return;
        }
    }
}

static GList *
find_timer_children_in_single_context (UProfContext *context,
                                       UProfTimerState *parent)
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

static GList *
find_timer_children (UProfContext *context, UProfTimerState *parent)
{
  GList *all_children = NULL;
  GList *children;
  GList *l;

#ifdef DEBUG_TIMER_HEIRARACHY
  GList *l2;
  g_print (" find_timer_children (context = %s, parent = %s):\n",
           context->name, ((UProfObjectState *)parent)->name);
#endif

  /* NB: links allow users to combine the timers and counters from one
   * context into another, so when searching for the children of any
   * given timer we also need to search any linked contexts... */

  children = find_timer_children_in_single_context (context, parent);
  all_children = g_list_concat (all_children, children);

#ifdef DEBUG_TIMER_HEIRARACHY
  g_print ("  direct children of %s:\n",
           parent ? ((UProfObjectState *)parent)->name: "NULL");
  for (l = all_children; l != NULL; l = l->next)
    g_print ("   name = %s\n", ((UProfObjectState *)l->data)->name);
#endif

  for (l = context->links; l != NULL; l = l->next)
    {
      children = find_timer_children_in_single_context (l->data, parent);
      all_children = g_list_concat (all_children, children);
#ifdef DEBUG_TIMER_HEIRACHY
      g_print ("  linked children of %s:\n", ((UProfContext *)l->data)->name);
      for (l2 = all_children; l2 != NULL; l2 = l2->next)
        g_print ("   name = %s\n", ((UProfObjectState *)l2->data)->name);
#endif
    }

  return all_children;
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

#ifdef DEBUG_TIMER_HEIRACHY
  g_print ("resolve_timer_heirachy (context = %s, timer = %s, parent = %s)\n",
           context->name,
           ((UProfObjectState *)timer)->name,
           parent ? ((UProfObjectState *)parent)->name : "NULL");
#endif

  timer->parent = parent;
  timer->children = find_timer_children (context, timer);

#ifdef DEBUG_TIMER_HEIRACHY
  g_print ("resolved children of %s:\n",
           timer ? ((UProfObjectState *)timer)->name: "NULL");
  for (l = timer->children; l != NULL; l = l->next)
    g_print ("  name = %s\n", ((UProfObjectState *)l->data)->name);
#endif

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

static int
get_name_size_for_timer_and_children (UProfTimerResult *timer,
                                      int indent_level)
{
  int max_name_size =
    (indent_level * 2) + strlen (UPROF_OBJECT_STATE (timer)->name);
  GList *l;

  for (l = timer->children; l; l = l->next)
    {
      UProfTimerResult *child = l->data;
      int child_name_size =
        get_name_size_for_timer_and_children (child, indent_level + 1);
      if (child_name_size > max_name_size)
        max_name_size = child_name_size;
    }

  return max_name_size;
}

static void
print_timer_and_children (UProfReport                   *report,
                          UProfTimerResult              *timer,
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
           report->max_timer_name_size + 1 - indent,
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

      print_timer_and_children (report,
                                child,
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
  UProfReport *report;

  if (callback)
    extra_titles = callback (NULL, &extra_titles_width, data);
  if (!extra_titles)
    extra_titles = g_strdup ("");

  report = g_new0 (UProfReport, 1);
  report->max_timer_name_size =
    get_name_size_for_timer_and_children (timer, 0);

  g_print (" %-*s%s %*s %s\n",
           report->max_timer_name_size + 1,
           "Name",
           "Total msecs",
           extra_titles_width,
           extra_titles,
           "Percent");

  g_free (extra_titles);

  print_timer_and_children (report, timer, callback, 0, data);
  g_free (report);
}

void
uprof_context_foreach_timer (UProfContext            *context,
                             GCompareDataFunc         sort_compare_func,
                             UProfTimerResultCallback callback,
                             gpointer                 data)
{
  GList *l;
  GList **timers;
  GList *all_timers = NULL;

  g_return_if_fail (context != NULL);
  g_return_if_fail (callback != NULL);

#ifdef DEBUG_TIMER_HEIRACHY
  g_print ("uprof_context_foreach_timer (context = %s):\n",
           context->name);
#endif

  /* If the context has been linked with other contexts, then we want
   * a flat list of timers we can sort... */
  /* XXX: may want a dirty flag mechanism to avoid repeating this
   * too often! */
  if (context->links)
    {
      all_timers = g_list_copy (context->timers);

#ifdef DEBUG_TIMER_HEIRACHY
      g_print (" all %s timers:\n", context->name);
      for (l = all_timers; l != NULL; l = l->next)
        g_print ("  timer->name: %s\n", ((UProfObjectState *)l->data)->name);
#endif

      for (l = context->links; l != NULL; l = l->next)
        {
          GList *linked_timers = ((UProfContext *)l->data)->timers;

#ifdef DEBUG_TIMER_HEIRACHY
          GList *l2;
          g_print (" all %s timers (linked):\n", ((UProfContext *)l->data)->name);
          for (l2 = linked_timers; l2 != NULL; l2 = l2->next)
            g_print ("  timer->name: %s\n", ((UProfObjectState *)l2->data)->name);
#endif

          linked_timers = g_list_copy (linked_timers);
          all_timers = g_list_concat (all_timers, linked_timers);
        }
      timers = &all_timers;
    }
  else
    timers = &context->timers;

#ifdef DEBUG_TIMER_HEIRACHY
  g_print (" all combined timers:\n");
  for (l = *timers; l != NULL; l = l->next)
    g_print ("  timer->name: %s\n", ((UProfObjectState *)l->data)->name);
#endif

  if (sort_compare_func)
    *timers = g_list_sort_with_data (*timers, sort_compare_func, data);
  for (l = *timers; l != NULL; l = l->next)
    callback (l->data, data);

  g_list_free (all_timers);
}

void
uprof_context_foreach_counter (UProfContext              *context,
                               GCompareDataFunc           sort_compare_func,
                               UProfCounterResultCallback callback,
                               gpointer                   data)
{
  GList *l, *l2;
  GList **counters;
  GList *all_counters = NULL;

  g_return_if_fail (context != NULL);
  g_return_if_fail (callback != NULL);

  /* If the context has been linked with other contexts, then we want
   * a flat list of counters we can sort... */
  /* XXX: may want a dirty flag mechanism to avoid repeating this
   * too often! */
  if (context->links)
    {
      all_counters = g_list_copy (context->counters);
      for (l = context->links; l != NULL; l = l->next)
        {
          l2 = ((UProfContext *)l->data)->counters;
          l2 = g_list_copy (l2);
          all_counters = g_list_concat (all_counters, l2);
        }
      counters = &all_counters;
    }
  else
    counters = &context->counters;

  if (sort_compare_func)
    *counters = g_list_sort_with_data (*counters, sort_compare_func, data);
  for (l = *counters; l != NULL; l = l->next)
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
  GList *root_timers;
  GList *l;

  g_print (" context: %s\n", context->name);
  g_print ("\n");
  g_print (" counters:\n");

  uprof_context_foreach_counter (context,
                                 UPROF_COUNTER_SORT_COUNT_INC,
                                 default_print_counter,
                                 NULL);

  g_print ("\n");
  g_print (" timers:\n");
  g_assert (context->resolved);
  root_timers = uprof_context_get_root_timer_results (context);
  for (l = context->root_timers; l != NULL; l = l->next)
    uprof_timer_result_print_and_children ((UProfTimerResult *)l->data,
                                           NULL, NULL);
}

static void
resolve_timer_heirachy_cb (UProfTimerResult *timer, void *data)
{
  UProfContext *context = data;
  if (timer->parent_name == NULL)
    {
      resolve_timer_heirachy (context, timer, NULL);
      context->root_timers = g_list_prepend (context->root_timers, timer);
    }
}

#ifdef DEBUG_TIMER_HEIRARACHY
static void
print_timer_recursive (UProfTimerResult *timer, int indent)
{
  UProfContext *context = uprof_timer_result_get_context (timer);
  GList *l;

  g_print ("%*scontext = %s, timer = %s, parent_name = %s, "
           "parent = %s\n",
           2 * indent, "",
           context->name,
           timer->object.name,
           timer->parent_name,
           timer->parent ? timer->parent->object.name : "NULL");
  for (l = timer->children; l != NULL; l = l->next)
    {
      UProfTimerResult *child = l->data;
      g_print ("%*schild = %s\n",
               2 * indent, "",
               child->object.name);
    }

  indent++;
  for (l = timer->children; l != NULL; l = l->next)
    print_timer_recursive (l->data, indent);
}

static void
print_timer_heirachy_cb (UProfTimerResult *timer, void *data)
{
  if (timer->parent == NULL)
    print_timer_recursive (timer, 0);
}
#endif

static void
uprof_context_resolve_timer_heirachy (UProfContext *context)
{
#ifdef DEBUG_TIMER_HEIRARACHY
  GList *l;
#endif

  if (context->resolved)
    return;

  g_assert (context->root_timers == NULL);

  /* Use the parent names of timers to resolve the actual parent
   * child hierarchy (fill in timer .children, and .parent members) */
  uprof_context_foreach_timer (context,
                               NULL, /* no need to sort */
                               resolve_timer_heirachy_cb,
                               context);

#ifdef DEBUG_TIMER_HEIRARACHY
  g_print ("resolved root_timers:\n");
  for (l = context->root_timers; l != NULL; l = l->next)
    g_print (" name = %s\n", ((UProfObjectState *)l->data)->name);

  uprof_context_foreach_timer (context,
                               NULL, /* no need to sort */
                               print_timer_heirachy_cb,
                               context);
#endif

  context->resolved = TRUE;
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

static void
_uprof_suspend_single_context (UProfContext *context)
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
uprof_suspend_context (UProfContext *context)
{
  GList *l;

  _uprof_suspend_single_context (context);

  for (l = context->links; l != NULL; l = l->next)
    _uprof_suspend_single_context (l->data);
}

static void
_uprof_resume_single_context (UProfContext *context)
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
uprof_resume_context (UProfContext *context)
{
  GList *l;

  _uprof_resume_single_context (context);

  for (l = context->links; l != NULL; l = l->next)
    _uprof_resume_single_context (l->data);
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

