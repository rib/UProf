/* This file is part of UProf.
 *
 * Copyright © 2010 Robert Bragg
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

#ifndef _UPROF_REPORT_H_
#define _UPROF_REPORT_H_

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define UPROF_TYPE_REPORT              (uprof_report_get_type ())
#define UPROF_REPORT(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), UPROF_TYPE_REPORT, UProfReport))
#define UPROF_REPORT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), UPROF_TYPE_REPORT, UProfReportClass))
#define UPROF_IS_REPORT(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), UPROF_TYPE_REPORT))
#define UPROF_IS_REPORT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), UPROF_TYPE_REPORT))
#define UPROF_REPORT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), UPROF_TYPE_REPORT, UProfReportClass))

#ifndef UPROF_REPORT_TYPEDEF
typedef struct _UProfReport UProfReport;
#define UPROF_REPORT_TYPEDEF
#endif
typedef struct _UProfReportClass UProfReportClass;
typedef struct _UProfReportPrivate UProfReportPrivate;

GType uprof_report_get_type (void);

struct _UProfReport
{
  GObject parent;
  UProfReportPrivate *priv;
};

struct _UProfReportClass
{
  GObjectClass parent;
};

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
 * uprof_report_get_name:
 * @report: A UProfReport object
 *
 * Returns the name of the given @report. See uprof_report_new().
 *
 * Returns: The name of the given @report.
 * Since: 0.4
 */
const char *
uprof_report_get_name (UProfReport *report);

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

#ifndef UPROT_COUNTER_RESULT_TYPEDEF
typedef struct _UProfCounterState UProfCounterResult;
#define UPROT_COUNTER_RESULT_TYPEDEF
#endif
#ifndef UPROT_TIMER_RESULT_TYPEDEF
typedef struct _UProfTimerState   UProfTimerResult;
#define UPROT_TIMER_RESULT_TYPEDEF
#endif

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

G_END_DECLS

#endif /* _UPROF_REPORT_H_ */

