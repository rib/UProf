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
#define REPORT_COLUMN0_WIDTH 50

struct _UProfContext
{
  guint  ref;

  char	*name;
  GList	*counters;
  GList	*timers;
  GList *root_timers;
};

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
   * For Core 2 Duo, or hyper threaded systems, then as I understand it the
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
  return system_counter_hz;
}

UProfContext *
uprof_context_new (const char *name)
{
  UProfContext *context = g_new0 (UProfContext, 1);
  context->ref = 1;

  context->name = g_strdup (name);
  return context;
}

void
uprof_context_ref (UProfContext *context)
{
  context->ref++;
}

void
uprof_context_unref (UProfContext *context)
{
  context->ref--;
  if (!context->ref)
    {
      g_free (context->name);
      g_list_free (context->counters);
      g_list_free (context->timers);
      g_free (context);
    }
}

void
uprof_context_add_counter (UProfContext *context, UProfCounter *counter)
{
  /* XXX: Check if we have actually seen this counter before; it might be that
   * it belongs to a dynamic shared object that has been reloaded */
  context->counters = g_list_prepend (context->counters, counter);
}

void
uprof_context_add_timer (UProfContext *context, UProfTimer *timer)
{
  /* XXX: Check if we have actually seen this counter before; it might be that
   * it belongs to a dynamic shared object that has been reloaded */
  context->timers = g_list_prepend (context->timers, timer);
}

static GList *
find_timer_children (UProfContext *context, UProfTimer *parent)
{
  GList *l;
  GList *children = NULL;

  for (l = context->timers; l != NULL; l = l->next)
    {
      UProfTimer *timer = l->data;
      if (timer->parent_name && strcmp (timer->parent_name, parent->name) == 0)
        children = g_list_prepend (children, timer);
    }
  return children;
}

gint
compare_timer_totals (UProfTimer *a, UProfTimer *b)
{
  if (a->total > b->total)
    return -1;
  else if (a->total < b->total)
    return 1;
  else
    return 0;
}

/*
 * Timer parents are declared using a string to name parents; this function
 * takes the names and resolves them to actual UProfTimer structures.
 */
static void
resolve_timer_heirachy (UProfContext *context,
                        UProfTimer *timer,
                        UProfTimer *parent)
{
  GList *l;
  timer->parent = parent;
  timer->children = find_timer_children (context, timer);
  for (l = timer->children; l != NULL; l = l->next)
    resolve_timer_heirachy (context, (UProfTimer *)l->data, timer);
}

static void
sort_timers (UProfContext *context)
{
  GList *l;

  context->timers = g_list_reverse (context->timers);

  /* First use the parent names of timers to resolve the actual parent
   * child heirachy (fill in timer .children, and .parent members) */
  for (l = context->timers; l != NULL; l = l->next)
    {
      UProfTimer *timer = l->data;
      if (timer->parent == NULL)
        {
          resolve_timer_heirachy (context, timer, NULL);
          context->root_timers = g_list_prepend (context->root_timers, timer);
        }
    }

  context->root_timers =
    g_list_sort (context->root_timers, (GCompareFunc)compare_timer_totals);
  for (l = context->timers; l != NULL; l = l->next)
    {
      UProfTimer *timer = l->data;
      timer->children =
        g_list_sort (timer->children, (GCompareFunc)compare_timer_totals);
    }
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

static UProfTimer *
get_root_timer (UProfTimer *timer)
{
  while (timer->parent)
    timer = timer->parent;

  return timer;
}

static void
print_timer_and_children (UProfContext *context,
                          UProfTimer *timer,
                          int indent_level)
{
  UProfTimer *root;
  GList *l;
  float percent;
  int bar_len, indent;

  /* percentages are reported relative to the root timer */
  root = get_root_timer (timer);

  indent = indent_level * 2; /* 2 spaces per indent level */

  percent = ((float)timer->total / (float)root->total) * 100.0;
  g_print (" %*s%-*s%-10.2f(msec), %7.3f%% ",
           indent,
           "",
           REPORT_COLUMN0_WIDTH - indent,
           timer->name,
           ((float)timer->total / system_counter_hz) * 1000.0,
           percent);

  for (bar_len = 3.6 * percent; bar_len >= 8; bar_len -= 8)
    g_printf ("%s", bars[8]);
  if (bar_len)
    g_printf ("%s", bars[bar_len]);
  g_print ("\n");

  for (l = timer->children; l != NULL; l = l->next)
    print_timer_and_children (context, (UProfTimer *)l->data, indent_level + 1);
}

void
uprof_context_output_report (UProfContext *context)
{
  GList *l;

  /* XXX: This needs to support stuff like applications being able to manually
   * iterate the UProf{Counter,Timer} structures for custom reporting. */

  uprof_calibrate_system_counter ();

  g_print ("\n");
  g_print ("UProf report:\n");
  g_print (" context: %s\n", context->name);
  g_print ("\n");
  g_print (" counters:\n");
  context->counters = g_list_reverse (context->counters);
  for (l = context->counters; l != NULL; l = l->next)
    {
      UProfCounter *counter = l->data;
      g_print ("   %-*s%-5ld\n", REPORT_COLUMN0_WIDTH - 2,
               counter->name, counter->count);
    }
  g_print ("\n");
  g_print (" timers:\n");
  sort_timers (context);
  for (l = context->root_timers; l != NULL; l = l->next)
    print_timer_and_children (context, (UProfTimer *)l->data, 1);
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

