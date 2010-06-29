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

#ifndef _UPROF_CONTEXT_H_
#define _UPROF_CONTEXT_H_

#include <uprof-counter.h>
#include <uprof-counter-result.h>
#include <uprof-timer.h>
#include <uprof-timer-result.h>

#include <glib.h>

G_BEGIN_DECLS

#ifndef UPROF_CONTEXT_TYPEDEF
typedef struct _UProfContext UProfContext;
#define UPROF_CONTEXT_TYPEDEF
#endif

/**
 * uprof_context_new:
 * @name: The top most name to categorize your profiling results
 *
 * This creates a new profiling context with a given name. After creating
 * a context you should declare your timers and counters, and once
 * you have accumulated data you can print the results by creating a report
 * via uprof_report_new(), uprof_report_add_context() and uprof_report_print()
 *
 * For example:
 * |[
 *    UProfContext *context;
 *    UProfReport *report;
 *    UPROF_STATIC_TIMER (parent_timer,
 *                        NULL, // no parent
 *                        "Parent timer",
 *                        "An example parent timer",
 *                        0 // no application private data);
 *    UPROF_STATIC_TIMER (timer,
 *                        "Parent timer",
 *                        "Simple timer",
 *                        "An example timer",
 *                        0 // no application private data);
 *
 *    uprof_context_init (argc, argv);
 *
 *    context = uprof_context_new ("Test Context");
 *
 *    UPROF_TIMER_START (context, parent_timer);
 *    sleep (1);
 *    UPROF_TIMER_START (context, timer);
 *    sleep (1);
 *    UPROF_TIMER_STOP (context, timer);
 *    UPROF_TIMER_STOP (context, parent_timer);
 *
 *    report = uprof_report_new ("Test Report");
 *    uprof_report_add_context (context);
 *    uprof_report_print ();
 *    uprof_report_unref ();
 *
 *    uprof_context_unref ();
 * ]|
 */
UProfContext *
uprof_context_new (const char *name);

/**
 * uprof_context_ref:
 * @context: A UProfContext
 *
 * Take a reference on a uprof context.
 */
UProfContext *
uprof_context_ref (UProfContext *context);

/**
 * uprof_context_imref:
 * @context: A UProfContext
 *
 * Release a reference on a uprof context. When the reference count reaches
 * zero the context is freed.
 */
void
uprof_context_unref (UProfContext *context);

/**
 * uprof_context_get_name:
 * @context: A UProfContext
 *
 * Returns the name of the given @context. See uprof_context_new().
 *
 * Returns: The name of the given @context.
 *
 * Since: 0.4
 */
const char *
uprof_context_get_name (UProfContext *context);

/**
 * uprof_context_add_counter:
 * @context: A UProfContext
 *
 * Declares a new uprof counter and associates it with a context. Normally this
 * API isn't used directly because the UPROF_COUNTER_INC(), UPROF_COUNTER_DEC()
 * and UPROF_COUNTER_ZERO() macros will ensure a counter is added the first
 * time it used.
 */
void
uprof_context_add_counter (UProfContext *context, UProfCounter *counter);

/**
 * uprof_context_add_timer:
 * @context: A UProfContext
 *
 * Declares a new uprof timer and associates it with a context. Normally this
 * API isn't used directly because the UPROF_TIMER_START() and
 * UPROF_RECURSIVE_TIMER_START() macros will ensure a timer is added the first
 * time it used.
 */
void
uprof_context_add_timer (UProfContext *context, UProfTimer *timer);

/**
 * uprof_context_link:
 * @context: A UProfContext
 * @other: A UProf context whos timers and counters you want to be made
 *         available to @context.
 *
 * Links two contexts together so the timers and counters of the
 * @other context will become in a way part of the first @context - at
 * least as far as reporting is concerned. For example calling
 * uprof_context_foreach_counter() would iterate all the counters of
 * other contexts linked to the given context.
 *
 * One example for linking contexts is for mainloop based libraries
 * that can optionally run under the control of an external mainloop.
 * In this case where it can't be predetermined who will own the
 * mainloop UProf provides a shared context just for the purpose of
 * tracking mainloop statistics which by convention everyone should
 * use. If you need to track mainloop statistics you should link
 * UProf's mainloop context into your own. See
 * uprof_get_mainloop_context() for more details.
 *
 * Another case where linking contexts can be useful is if you are
 * profiling a library that itself dynamically loads a shared object
 * (DSO), but you can't export a context symbol from the library to
 * the DSO because it happens that this DSO is also used by other
 * libraries or applications which can't provide that symbol.
 *
 * The idea is that the DSO can create its own UProf context without
 * depending on a symbol being exported from the library and then that
 * library can get a handle on the statistics collected by that
 * library at runtime via UProf and link the DSO context into the
 * library's context for reporting.
 *
 * An example of this is profiling a DRI driver which can be loaded by X
 * clients for direct rendering, or also by the X server for indirect
 * rendering. If you try and export a context variable from the GL driver to
 * the DRI driver there will end up being an unresolved symbol when the X
 * server tries to load the driver.
 */
void
uprof_context_link (UProfContext *context, UProfContext *other);

/**
 * uprof_context_unlink:
 * @context: A UProfContext
 * @other: A UProf context whos timers and counters were previously made
 *         available to @context.
 *
 * This removes a link between two contexts that was previously created using
 * uprof_context_link()
 */
void
uprof_context_unlink (UProfContext *context, UProfContext *other);

/* XXX: deprecated */
/**
 * uprof_context_output_report:
 * @context: A UProfContext
 *
 * Generates a report of the accumulated timer and counter data associated with
 * the given context.
 */
void
uprof_context_output_report (UProfContext *context);

/**
 * uprof_context_add_report_message:
 * @context: A UProfContext
 * @format: A printf style format string
 *
 * This function queues a message to be output when uprof generates its
 * final report.
 */
void
uprof_context_add_report_message (UProfContext *context,
                                  const char *format, ...);
/**
 * uprof_context_suspend:
 * @context: A UProfContext
 *
 * Disables all timer and counter accounting for a context and all linked
 * contexts. This can be used to precisely control what you profile. For
 * example in Clutter if we want to focus on input handling we suspend the
 * context early on during library initialization and only resume it while
 * processing input.
 *
 * You can suspend a context multiple times, and it will only resume with an
 * equal number of calls to uprof_context_resume()
 */
void
uprof_context_suspend (UProfContext *context);

/**
 * uprof_context_resume:
 * @context: A uprof context
 *
 * Re-Enables all timer and counter accounting for a context (and all linked
 * contexts) if previously disabled with a call to uprof_context_suspend().
 *
 * You can suspend a context multiple times, and it will only resume with an
 * equal number of calls to uprof_context_resume()
 */
void
uprof_context_resume (UProfContext *context);

GList *
uprof_context_get_root_timer_results (UProfContext *context);

typedef void (*UProfCounterResultCallback) (UProfCounterResult *counter,
                                            gpointer            data);

void
uprof_context_foreach_counter (UProfContext              *context,
                               GCompareDataFunc           sort_compare_func,
                               UProfCounterResultCallback callback,
                               gpointer                   data);

typedef void (*UProfTimerResultCallback) (UProfTimerResult *timer,
                                          gpointer          data);

void
uprof_context_foreach_timer (UProfContext            *context,
                             GCompareDataFunc         sort_compare_func,
                             UProfTimerResultCallback callback,
                             gpointer                 data);

UProfCounterResult *
uprof_context_get_counter_result (UProfContext *context, const char *name);

UProfTimerResult *
uprof_context_get_timer_result (UProfContext *context, const char *name);

/* XXX: deprecated */
/**
 * uprof_context_get_messages:
 * @context: A UProfContext
 *
 * Returns: a list of messages previously logged using
 * uprof_context_add_report_message()
 */
GList *
uprof_context_get_messages (UProfContext *context);

G_END_DECLS

#endif /* _UPROF_CONTEXT_H_ */

