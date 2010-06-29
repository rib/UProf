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
#ifndef USE_RDTSC
static clockid_t clockid;
#endif

GList *_uprof_all_contexts;
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
uprof_get_mainloop_context (void)
{
  return mainloop_context;
}

UProfContext *
uprof_find_context (const char *name)
{
  GList *l;
  for (l = _uprof_all_contexts; l != NULL; l = l->next)
    if (strcmp (uprof_context_get_name (l->data), name) == 0)
      return l->data;
  return NULL;
}

