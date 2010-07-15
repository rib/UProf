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

#ifndef _UPROF_H_
#define _UPROF_H_

#include <uprof-context.h>
#include <uprof-counter.h>
#include <uprof-counter-result.h>
#include <uprof-timer.h>
#include <uprof-timer-result.h>
#include <uprof-report.h>
#include <uprof-dbus.h>
#include <uprof-report-proxy.h>

#include <glib.h>

#include <stdint.h>

G_BEGIN_DECLS

/**
 * SECTION:uprof
 * @short_description: Fuctions to initialize uprof + query the system counter
 *
 * UProf is a toolkit for profiling applications and libraries with an
 * emphasis on domain specific instrumentation. Unlike tools such as
 * OProfile or Sysprof UProf can be used to provide application
 * specific reports making statistics more accesible, and encouraging
 * ongoing tracking of key statistics. Also unlike sysprof the current
 * timing features are non-stochastic and measure real world elapsed
 * time which can be a particularly helpful way of highlighiting
 * non-CPU bound bottlenecks.
 *
 * To give a taster of how you can add UProf instrumentation to your
 * application here is a simple example:
 *
 * |[
 * #include <uprof.h>
 * #include <unistd.h>
 *
 * UPROF_STATIC_TIMER (timer,
 *                     NULL, /<!-- -->* parent *<!-- -->/
 *                     "Example timer",
 *                     "An example timer around a sleep(1)",
 *                     0); /<!-- -->* private data *<!-- -->/
 *
 * /<!-- -->* Each application/library typically has one context symbol
 *    visible across the whole application. *<!-- -->/
 * UProfContext *_my_context;
 *
 * int
 * main (int argc, char **argv)
 * {
 *   UProfReport *report;
 *
 *   uprof_init (&argc, &argv);
 *
 *   _my_context = uprof_context_new ("My Context");
 *
 *   UPROF_TIMER_START (_my_context, timer);
 *   sleep (1);
 *   UPROF_TIMER_STOP (_my_context, timer);
 *
 *   report = uprof_report_new ("Example report");
 *   uprof_report_add_context (report, _my_context);
 *   uprof_report_print (report);
 *   uprof_report_unref (report);
 *
 *   uprof_context_unref (_my_context);
 * }
 * ]|
 *
 * So once you have initialized uprof (normally using uprof_init())
 * you should take a look at uprof_context_new().
 */

/**
 * uprof_init:
 * @argc: (inout): The number of arguments in @argv
 * @argv: (array length=argc) (inout) (allow-none): A pointer to an array
 *   of arguments.
 *
 * It will initialise everything needed to operate with UProf and
 * parses any standard command line options. @argc and @argv are
 * adjusted accordingly so your own code will never see those standard
 * arguments.
 */
void
uprof_init (int *argc, char ***argv);

/**
 * uprof_get_option_group:
 *
 * Returns a #GOptionGroup for the command line arguments recognized
 * by UProf. You should add this group to your #GOptionContext with
 * g_option_context_add_group(), if you are using g_option_context_parse()
 * to parse your commandline arguments.
 *
 * Calling g_option_context_parse() with UProf's #GOptionGroup will result
 * in UProf's initialization. That is, the following code:
 *
 * |[
 *   g_option_context_set_main_group (context, uprof_get_option_group ());
 *   res = g_option_context_parse (context, &amp;argc, &amp;argc, NULL);
 * ]|
 *
 * can be used to replace:
 *
 * |[
 *   uprof_init (&amp;argc, &amp;argv);
 * ]|
 *
 * A notable difference to calling uprof_init() directly though is
 * that it's your responsibility to also add the #GOptionGroup<!--
 * -->s for any of UProf's dependencies if you want to expose their
 * configuration options.
 *
 * After g_option_context_parse() on a #GOptionContext containing the
 * UProf #GOptionGroup has returned %TRUE, UProf is guaranteed to be
 * initialized
 *
 * Return value: (transfer full): a #GOptionGroup for the commandline
 *   arguments recognized by UProf
 *
 * Since: 0.4
 */
GOptionGroup *
uprof_get_option_group (void);

/**
 * uprof_get_system_counter:
 *
 * Gives direct access to the counter that uprof is using for timing.  On x86
 * platforms this executes the rdtsc instruction to return a 64bit integer that
 * increases at the CPU or system bus frequency. Other platforms fall back to
 * clock_gettime (CLOCK_MONOTONIC, &ts)
 *
 * Returns: a 64bit system counter
 */
guint64
uprof_get_system_counter (void);

/**
 * uprof_get_system_counter_hz:
 *
 * Allows you to convert elapsed counts into seconds. Be aware that the
 * calculation of the conversion factor is done in a fairly crude way so it may
 * not be very accurate. This usually isn't a big problem though as any
 * inaccuracy will apply consistently to everything in a uprof report so the
 * numbers still tend to end up relatively useful. It may be worth bearing in
 * mind though if comparing different reports.
 *
 * Returns: A factor that can be used to convert elapsed counts into seconds.
 */
guint64
uprof_get_system_counter_hz (void);

/**
 * uprof_find_context:
 * @name: Find an existing uprof context by name
 *
 * This function looks for a context by name which - for example - may
 * then be passed to uprof_context_link.
 */
UProfContext *
uprof_find_context (const char *name);

/**
 * uprof_get_mainloop_context:
 *
 * Returns the shared mainloop context which should be used by any
 * code wanting to instrument its mainloop with timers.
 *
 * Because some libraries can either control their own mainloop or
 * alternatively run under the control of an external mainloop UProf
 * provides a shared context that is created during uprof_init () that
 * can be linked into your application context just for the purpose of
 * tracking mainloop statistics.
 *
 * If all components follow the convention of naming their mainloop
 * timer "Mainloop" and their corresponding idle timer "Mainloop Idle"
 * you can be sure that mainloop statistics can always be found in the
 * same place regardless of who ends up owning the mainloop.
 *
 * Your application can declare still declare a mainloop timer using
 * the usual macros like:
 * |[
 * CLUTTER_STATIC_TIMER (mainloop_timer,
 *                       NULL, //no parent
 *                       "Mainloop",
 *                       "The time spent in the clutter mainloop",
 *                       0);  // no application private data
 * ]|
 *
 * but you would then start and stop the timer like:
 * |[
 * CLUTTER_TIMER_START (uprof_get_mainloop_context (), mainloop_timer);
 * CLUTTER_TIMER_STOP (uprof_get_mainloop_context (), mainloop_timer);
 * ]|
 *
 * Each uprof context that contains timers depending on the mainloop
 * timer should make sure to link the mainloop context into their own
 * context using uprof_context_link().
 *
 * Returns: A pointer to the shared mainloop context. Don't unref this
 * context.
 *
 * Since: 0.4
 */
UProfContext *
uprof_get_mainloop_context (void);

G_END_DECLS

#endif /* _UPROF_H_ */

