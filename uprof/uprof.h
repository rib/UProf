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

#include <uprof-private.h>

#include <glib.h>
#include <stdint.h>

G_BEGIN_DECLS

typedef struct _UProfCounter
{
  /** Application defined name for counter */
  const char *name;

  /** Application defined description for counter */
  const char *description;

  /** Application private data */
  unsigned long priv;

  /** private */
  UProfCounterState *state;

  const char *filename;
  unsigned long line;
  const char *function;

} UProfCounter;

typedef struct _UProfTimer
{
  /** Application defined name for timer */
  const char *name;

  /** Application defined description for timer */
  const char *description;

  /** Application defined parent */
  const char *parent_name;

  /** Application private data */
  unsigned long priv;

  /** private */
  UProfTimerState *state;

  const char *filename;
  unsigned long line;
  const char *function;

} UProfTimer;

typedef struct _UProfCounterState UProfCounterResult;
typedef struct _UProfTimerState   UProfTimerResult;

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
 * uprof_find_context:
 * @name: Find an existing uprof context by name
 *
 * This function looks for a context by name which - for example - may
 * then be passed to uprof_context_link.
 */
UProfContext *
uprof_find_context (const char *name);

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
 * Links two contexts together so the timers and counters of the @other context
 * will become in a way part of the first @context - at least as far as
 * reporting is concerned. For example calling uprof_context_foreach_counter()
 * would iterate all the counters of other contexts linked to the given
 * context.
 *
 * This can be useful if you are profiling a library that itself dynamically
 * loads a shared object (DSO), but you can't export a context symbol from the
 * library to the DSO because it happens that this DSO is also used by other
 * libraries or applications which can't provide that symbol.
 *
 * The intention is to create a context that is owned by the DSO, and when we
 * end up loading the DSO in the library we are profiling we can link that
 * context into the real context.
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

/**
 * uprof_context_output_report:
 * @context: A UProfContext
 *
 * Generates a report of the accumulated timer and counter data associated with
 * the given context.
 */
void
uprof_context_output_report (UProfContext *context);

/************
 * Counters
 ************/

/**
 * UPROF_COUNTER:
 * @COUNTER_SYMBOL: The name of the C symbol to declare
 * @NAME: The name of the counter used for reporting
 * @DESCRIPTION: A string describing what the timer represents
 * @PRIV: Optional private data (unsigned long) which you can access if you are
 *	  generating a very customized report. For example you might put
 *	  application specific flags here that affect reporting.
 *
 * Declares a new counter structure which can be used with the
 * UPROF_COUNTER_INC(), UPROF_COUNTER_DEC() and UPROF_COUNTER_ZERO() macros.
 * Usually you should use UPROF_STATIC_COUNTER() instead, but this may be useful
 * for special cases where counters are accessed from multiple files.
 */
#define UPROF_COUNTER(COUNTER_SYMBOL, NAME, DESCRIPTION, PRIV) \
  UProfCounter COUNTER_SYMBOL = { \
    .name = NAME, \
    .description = DESCRIPTION, \
    .priv = (unsigned long)(PRIV), \
    .state = NULL \
  }

/**
 * UPROF_STATIC_COUNTER:
 * @COUNTER_SYMBOL: The name of the C symbol to declare
 * @NAME: The name of the counter used for reporting
 * @DESCRIPTION: A string describing what the timer represents
 * @PRIV: Optional private data (unsigned long) which you can access if you are
 *	  generating a very customized report. For example you might put
 *	  application specific flags here that affect reporting.
 *
 * Declares a new static counter structure which can be used with the
 * UPROF_COUNTER_INC(), UPROF_COUNTER_DEC() and UPROF_COUNTER_ZERO() macros.
 */
#define UPROF_STATIC_COUNTER(COUNTER_SYMBOL, NAME, DESCRIPTION, PRIV) \
  static UPROF_COUNTER(COUNTER_SYMBOL, NAME, DESCRIPTION, PRIV)

#define _UPROF_INIT_UNSEEN_COUNTER(CONTEXT, COUNTER_SYMBOL) \
  do { \
    (COUNTER_SYMBOL).filename = __FILE__; \
    (COUNTER_SYMBOL).line = __LINE__; \
    (COUNTER_SYMBOL).function = __FUNCTION__; \
    uprof_context_add_counter (CONTEXT, &(COUNTER_SYMBOL)); \
  } while (0)

/**
 * UPROF_COUNTER_INC:
 * @CONTEXT: A UProfContext
 * @COUNTER_SYMBOL: A counter variable
 *
 * Increases the count for the given @COUNTER_SYMBOL.
 */
#define UPROF_COUNTER_INC(CONTEXT, COUNTER_SYMBOL) \
  do { \
    if (!(COUNTER_SYMBOL).state) \
      _UPROF_INIT_UNSEEN_COUNTER (CONTEXT, COUNTER_SYMBOL); \
    if ((COUNTER_SYMBOL).state->disabled) \
      break; \
    (COUNTER_SYMBOL).state->count++; \
  } while (0)

/**
 * UPROF_COUNTER_DEC:
 * @CONTEXT: A UProfContext
 * @COUNTER_SYMBOL: A counter variable
 *
 * Decreases the count for the given @COUNTER_SYMBOL.
 */
#define UPROF_COUNTER_DEC(CONTEXT, COUNTER_SYMBOL) \
  do { \
    if (!(COUNTER_SYMBOL).state) \
      _UPROF_INIT_UNSEEN_COUNTER (CONTEXT, COUNTER_SYMBOL) \
    if ((COUNTER_SYMBOL).state->disabled) \
      break; \
    (COUNTER_SYMBOL).state->count--; \
  } while (0)

/**
 * UPROF_COUNTER_ZERO:
 * @CONTEXT: A UProfContext
 * @COUNTER_SYMBOL: A counter variable
 *
 * Resets the count for the given @COUNTER_SYMBOL.
 */
#define UPROF_COUNTER_ZERO(CONTEXT, COUNTER_SYMBOL) \
  do { \
    if (!(COUNTER_SYMBOL).state) \
      _UPROF_INIT_UNSEEN_COUNTER (CONTEXT, COUNTER_SYMBOL) \
    if ((COUNTER_SYMBOL).state->disabled) \
      break; \
    (COUNTER_SYMBOL).state->count = 0; \
  } while (0)




/************
 * Timers
 ************/

/**
 * UPROF_STATIC_TIMER:
 * @TIMER_SYMBOL: The name of the C symbol to declare
 * @PARENT: The name of a parent timer (To clarify; this should be a string
 *          name identifying the parent, not the C symbol name for the parent)
 * @NAME: The timer name used for reporting
 * @DESCRIPTION: A string describing what the timer represents
 * @PRIV: Optional private data (unsigned long) which you can access if you are
 *	  generating a very customized report. For example you might put
 *	  application specific flags here that affect reporting.
 *
 * This can be used to declare a new timer structure which can then
 * be used with the UPROF_TIMER_START() and UPROF_TIMER_STOP() macros.
 * Usually you should use UPROF_STATIC_TIMER() instead but this can be useful
 * for special cases where you need to access a timer from multiple files.
 */
#define UPROF_TIMER(TIMER_SYMBOL, PARENT, NAME, DESCRIPTION, PRIV) \
  UProfTimer TIMER_SYMBOL = { \
    .name = NAME, \
    .description = DESCRIPTION, \
    .parent_name = PARENT, \
    .priv = (unsigned long)(PRIV), \
    .state = NULL \
  }

/**
 * UPROF_STATIC_TIMER:
 * @TIMER_SYMBOL: The name of the C symbol to declare
 * @PARENT: The name of a parent timer (it should really be the name given to
 *	    the parent, not the C symbol name for the parent)
 * @NAME: The timer name used for reporting
 * @DESCRIPTION: A string describing what the timer represents
 * @PRIV: Optional private data (unsigned long) which you can access if you are
 *	  generating a very customized report. For example you might put
 *	  application specific flags here that affect reporting.
 *
 * This can be used to declare a new static timer structure which can then
 * be used with the UPROF_TIMER_START() and UPROF_TIMER_STOP() macros.
 */
#define UPROF_STATIC_TIMER(TIMER_SYMBOL, PARENT, NAME, DESCRIPTION, PRIV) \
  static UPROF_TIMER(TIMER_SYMBOL, PARENT, NAME, DESCRIPTION, PRIV)

#define _UPROF_INIT_UNSEEN_TIMER(CONTEXT, TIMER_SYMBOL) \
  do { \
    (TIMER_SYMBOL).filename = __FILE__; \
    (TIMER_SYMBOL).line = __LINE__; \
    (TIMER_SYMBOL).function = __FUNCTION__; \
    uprof_context_add_timer (CONTEXT, &(TIMER_SYMBOL)); \
  } while (0)

#ifdef UPROF_DEBUG
#define DEBUG_CHECK_FOR_RECURSION(CONTEXT, TIMER_SYMBOL) \
  do { \
    if ((TIMER_SYMBOL).state->start) \
      { \
        g_warning ("Recursive starting of timer (%s) unsupported! \n" \
                   "You should use UPROF_RECURSIVE_TIMER_START if you " \
                   "need recursion", \
                   (TIMER_SYMBOL).name); \
      } \
  } while (0)
#else
#define DEBUG_CHECK_FOR_RECURSION(CONTEXT, TIMER_SYMBOL)
#endif

/**
 * UPROF_TIMER_START:
 * CONTEXT: A UProfContext
 * TIMER_SYMBOL: A timer variable
 *
 * Starts the timer timing. It is an error to try and start a timer that
 * is already timing; if you need to do this you should use
 * UPROF_RECURSIVE_TIMER_START() instead.
 */
#define UPROF_TIMER_START(CONTEXT, TIMER_SYMBOL) \
  do { \
    if (!(TIMER_SYMBOL).state) \
      _UPROF_INIT_UNSEEN_TIMER (CONTEXT, TIMER_SYMBOL); \
    DEBUG_CHECK_FOR_RECURSION (CONTEXT, TIMER_SYMBOL); \
    (TIMER_SYMBOL).state->start = uprof_get_system_counter (); \
  } while (0)

/**
 * UPROF_RECURSIVE_TIMER_START:
 * CONTEXT: A UProfContext
 * TIMER_SYMBOL: A timer variable
 *
 * Starts the timer timing like UPROF_TIMER_START() but with additional guards
 * to allow the timer to be started multiple times, such that
 * UPROF_RECURSIVE_TIMER_STOP() must be called an equal number of times to
 * actually stop it timing.
 */
#define UPROF_RECURSIVE_TIMER_START(CONTEXT, TIMER_SYMBOL) \
  do { \
    if (!(TIMER_SYMBOL).state) \
      _UPROF_INIT_UNSEEN_TIMER (CONTEXT, TIMER_SYMBOL); \
    if ((TIMER_SYMBOL).state->recursion++ == 0) \
      { \
        (TIMER_SYMBOL).state->start = uprof_get_system_counter (); \
      } \
  } while (0)

#ifdef UPROF_DEBUG
#define DEBUG_CHECK_TIMER_WAS_STARTED(CONTEXT, TIMER_SYMBOL) \
  do { \
    if (!(TIMER_SYMBOL).state->start) \
      { \
        g_warning ("Stopping an un-started timer! (%s)", \
                   (TIMER_SYMBOL).name); \
      } \
  } while (0)
#else
#define DEBUG_CHECK_TIMER_WAS_STARTED(CONTEXT, TIMER_SYMBOL)
#endif

#define COMPARE_AND_ADD_DURATION_TO_TOTAL(TIMER_SYMBOL) \
  do { \
    if (G_UNLIKELY (duration < (TIMER_SYMBOL).state->fastest)) \
      (TIMER_SYMBOL).state->fastest = duration; \
    else if (G_UNLIKELY (duration > (TIMER_SYMBOL).state->slowest)) \
      (TIMER_SYMBOL).state->slowest = duration; \
    (TIMER_SYMBOL).state->total += duration; \
  } while (0)

/**
 * UPROF_TIMER_STOP:
 * CONTEXT: A UProfContext
 * TIMER_SYMBOL: A timer variable
 *
 * Stops the timer timing. It is an error to try and stop a timer that
 * isn't actually timing. It is also an error to use this macro if the
 * timer was started using UPROF_RECURSIVE_TIMER_START(); you should
 * use UPROF_RECURSIVE_TIMER_STOP() instead.
 */
#define UPROF_TIMER_STOP(CONTEXT, TIMER_SYMBOL) \
  do { \
    DEBUG_CHECK_TIMER_WAS_STARTED (CONTEXT, TIMER_SYMBOL); \
    if (G_LIKELY (!(TIMER_SYMBOL).state->disabled)) \
      { \
        guint64 duration = uprof_get_system_counter() - \
                           (TIMER_SYMBOL).state->start + \
                           (TIMER_SYMBOL).state->partial_duration; \
        COMPARE_AND_ADD_DURATION_TO_TOTAL (TIMER_SYMBOL); \
      } \
    else if (G_UNLIKELY ((TIMER_SYMBOL).state->partial_duration)) \
      { \
        guint64 duration = (TIMER_SYMBOL).state->partial_duration; \
        COMPARE_AND_ADD_DURATION_TO_TOTAL (TIMER_SYMBOL); \
      } \
    (TIMER_SYMBOL).state->count++; \
    (TIMER_SYMBOL).state->start = 0; \
  } while (0)

/**
 * UPROF_RECURSIVE_TIMER_STOP:
 * CONTEXT: A UProfContext
 * TIMER_SYMBOL: A timer variable
 *
 * Stops a recursive timer timing. It is an error to try and stop a timer that
 * isn't actually timing. It is also an error to use this macro if the timer
 * was started using UPROF_TIMER_START(); you should use UPROF_TIMER_STOP()
 * instead.
 */
#define UPROF_RECURSIVE_TIMER_STOP(CONTEXT, TIMER_SYMBOL) \
  do { \
    g_assert ((TIMER_SYMBOL).state->recursion > 0); \
    if ((TIMER_SYMBOL).state->recursion-- == 1) \
      { \
        UPROF_TIMER_STOP (CONTEXT, TIMER_SYMBOL); \
      } \
  } while (0)

/* XXX: We should consider system counter wrap around issues */

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

/*
 * Support for reporting results:
 */

/**
 * uprof_report_new:
 * @name: The name of the report
 *
 * Creates an object representing a UProf report. You should associate the
 * contexts that you want to generate a report for with this object using
 * uprof_report_add_context() before generating a report via
 * uprof_report_print()
 *
 * Returns: A new UProfReport object
 */
UProfReport *
uprof_report_new (const char *name);

/**
 * uprof_report_ref:
 * @report: A UProfReport object
 *
 * Increases the reference count of a UProfReport object.
 */
UProfReport *
uprof_report_ref (UProfReport *report);

/**
 * uprof_report_unref:
 * @report: A UProfReport object
 *
 * Decreases the reference count of a UProfReport object. When the reference
 * count reaches 0 its associated resources will be freed.
 */
void
uprof_report_unref (UProfReport *report);

/**
 * uprof_report_add_context:
 * @report: A UProfReport object
 * @context: a UProfContext object
 *
 * Associates a context with a report object so that when the report is
 * generated it will include statistics relating to this context.
 */
void
uprof_report_add_context (UProfReport *report,
                          UProfContext *context);

typedef void (*UProfReportContextCallback) (UProfReport *report,
                                            UProfContext *context);

void
uprof_report_add_context_callback (UProfReport *report,
                                   UProfReportContextCallback callback);

void
uprof_report_remove_context_callback (UProfReport *report,
                                      UProfReportContextCallback callback);

/**
 * UProfReportCountersAttributeCallback:
 * @result: The current counter being reported
 * @user_data: The private data previously passed to
 *             uprof_report_add_counters_attribute()
 *
 * Use this prototype for the custom attribute callbacks. The string values
 * that you return in your callback may be wrapped across multiple lines and
 * uprof should still tabulate the report correctly. The
 */
typedef char *(*UProfReportCountersAttributeCallback) (UProfCounterResult *result,
                                                       void *user_data);

/**
 * uprof_report_add_counters_attribute:
 * @report: A UProfReport
 * @attribute_name: The name of the attribute
 * @callback: A function called for each counter being reported
 * @user_data: Private data passed back to your callback
 *
 * Adds a custom attribute/column to reports of counter statistics. The
 * attribute name can be wrapped with newline characters and the callback
 * functions will be called once for each counter result reported while using
 * uprof_report_print()
 *
 * The string values that you return in your callback may be wrapped across
 * multiple lines and uprof should still tabulate the report correctly. The
 * values should be freeable with g_free().
 *
 * For example:
 * |[
 * static char *double_count_cb (UProfCounterResult *counter, void *user_data)
 * {
 *   int count = uprof_counter_result_get_count (counter);
 *   return g_strdup_printf ("%d", count * 2);
 * }
 *
 * *snip*
 *
 * report = uprof_report_new ();
 * report = uprof_report_new ("Simple report");
 * uprof_report_add_counters_attribute (report, "Double\ncount",
 *                                      double_count_cb, NULL);
 * uprof_report_add_context (report, context);
 * uprof_report_print (report);
 * uprof_report_unref (report);
 * ]|
 *
 * Since: 0.4
 */
void
uprof_report_add_counters_attribute (UProfReport *report,
                                     const char *attribute_name,
                                     UProfReportCountersAttributeCallback callback,
                                     void *user_data);

/**
 * uprof_report_remove_counters_attribute:
 * @report: A UProfReport
 * @id: The custom report column you want to remove
 *
 * Removes the custom counters @attribute from future reports generated
 * with uprof_context_output_report()
 */
void
uprof_report_remove_counters_attribute (UProfReport *report,
                                        const char *attribute_name);

/**
 * UProfReportTimersAttributeCallback:
 * @result: The current timer being reported
 * @user_data: The private data previously passed to
 *             uprof_report_add_timers_attribute()
 *
 * Use this prototype for the custom attribute callbacks. The string values
 * that you return in your callback may be wrapped across multiple lines and
 * uprof should still tabulate the report correctly. The
 */
typedef char *(*UProfReportTimersAttributeCallback) (UProfTimerResult *result,
                                                     void *user_data);

/**
 * uprof_report_add_timers_attribute:
 * @report: A UProfReport
 * @attribute_name: The name of the attribute
 * @callback: A function called for each timer being reported
 * @user_data: Private data passed back to your callback
 *
 * Adds a custom attribute/column to reports of timer statistics. The attribute
 * name can be wrapped with newline characters and the callback functions will
 * be called once for each timer result reported while using
 * uprof_report_print()
 *
 * The string values that you return in your callback may be wrapped across
 * multiple lines and uprof should still tabulate the report correctly. The
 * values should be freeable with g_free().
 *
 * For example:
 * |[
 * static char *half_time_cb (UProfTimerResult *timer, void *user_data)
 * {
 *   gulong msecs = uprof_timer_result_get_total_msecs (timer);
 *   return g_strdup_printf ("%lu", msecs/2);
 * }
 *
 * *snip*
 *
 * report = uprof_report_new ();
 * report = uprof_report_new ("Simple report");
 * uprof_report_add_timers_attribute (report, "Half\ntime",
 *                                    half_time_cb, NULL);
 * uprof_report_add_context (report, context);
 * uprof_report_print (report);
 * uprof_report_unref (report);
 * ]|
 *
 * Since: 0.4
 */
void
uprof_report_add_timers_attribute (UProfReport *report,
                                   const char *attribute_name,
                                   UProfReportTimersAttributeCallback callback,
                                   void *user_data);

/**
 * uprof_report_remove_timers_attribute:
 * @report: A UProfReport
 * @id: The custom report column you want to remove
 *
 * Removes the custom timers @attribute from future reports generated with
 * uprof_context_output_report()
 */
void
uprof_report_remove_timers_attribute (UProfReport *report,
                                      const char *attribute_name);

void
uprof_report_print (UProfReport *report);

GList *
uprof_context_get_root_timer_results (UProfContext *context);

gint
_uprof_timer_compare_total_times (UProfTimerState *a,
                                  UProfTimerState *b,
                                  gpointer data);
gint
_uprof_timer_compare_start_count (UProfTimerState *a,
                                  UProfTimerState *b,
                                  gpointer data);
#define UPROF_TIMER_SORT_TIME_INC \
  ((GCompareDataFunc)_uprof_timer_compare_total_times)
#define UPROF_TIMER_SORT_COUNT_INC \
  ((GCompareDataFunc)_uprof_timer_compare_start_count)

gint
_uprof_counter_compare_count (UProfCounterState *a,
                              UProfCounterState *b,
                              gpointer data);
#define UPROF_COUNTER_SORT_COUNT_INC \
  ((GCompareDataFunc)_uprof_counter_compare_count)


typedef void (*UProfTimerResultCallback) (UProfTimerResult *timer,
                                          gpointer          data);

void
uprof_context_foreach_timer (UProfContext            *context,
                             GCompareDataFunc         sort_compare_func,
                             UProfTimerResultCallback callback,
                             gpointer                 data);


typedef void (*UProfCounterResultCallback) (UProfCounterResult *counter,
                                            gpointer            data);

void
uprof_context_foreach_counter (UProfContext              *context,
                               GCompareDataFunc           sort_compare_func,
                               UProfCounterResultCallback callback,
                               gpointer                   data);

UProfTimerResult *
uprof_context_get_timer_result (UProfContext *context, const char *name);

const char *
uprof_timer_result_get_description (UProfTimerResult *timer);

float
uprof_timer_result_get_total_msecs (UProfTimerResult *timer);

gulong
uprof_timer_result_get_start_count (UProfTimerResult *timer);

UProfTimerResult *
uprof_timer_result_get_parent (UProfTimerResult *timer);

UProfTimerResult *
uprof_timer_result_get_root (UProfTimerResult *timer);

GList *
uprof_timer_result_get_children (UProfTimerResult *timer);

UProfContext *
uprof_timer_result_get_context (UProfTimerResult *timer);

const char *
uprof_counter_result_get_name (UProfCounterResult *counter);

gulong
uprof_counter_result_get_count (UProfCounterResult *counter);

UProfContext *
uprof_counter_result_get_context (UProfCounterResult *counter);

void
uprof_print_percentage_bar (float percent);

typedef char * (*UProfTimerResultPrintCallback) (UProfTimerState *timer,
                                                 guint           *fields_width,
                                                 gpointer         data);

void
uprof_timer_result_print_and_children (UProfTimerResult              *timer,
                                       UProfTimerResultPrintCallback  callback,
                                       gpointer                       data);

/**
 * uprof_context_get_messages:
 * @context: A UProfContext
 *
 * Returns: a list of messages previously logged using
 * uprof_context_add_report_message()
 */
GList *
uprof_context_get_messages (UProfContext *context);

/* XXX: We might want the ability to declare "alias" timers so that we
 * can track multiple (filename, line, function) constants for essentially
 * the same timer. */

/* XXX: It would be nice if we could find a way to generalize how
 * relationships between timers and counters are made so we don't need
 * specialized reporting code to handle these use cases:
 *
 * - Having a frame_counter, and then other counters reported relative
 *   to the frame counter; such as averages of minor counters per frame.
 *
 * - Being able to report timer stats in relation to a counter (such as
 *   a frame counter). For example reporting the average time spent
 *   painting, per frame.
 */

/* XXX: static globals shouldn't be the only way to declare timers and
 * counters; exported globals or malloc'd variables might be handy too.
 */

G_END_DECLS

#endif /* _UPROF_H_ */

