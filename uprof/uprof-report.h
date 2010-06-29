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

#include <uprof-counter-result.h>
#include <uprof-timer-result.h>

#include <glib-object.h>
#include <glib.h>

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
 * @report: A #UProfReport object
 *
 * Increases the reference count of a UProfReport object.
 */
UProfReport *
uprof_report_ref (UProfReport *report);

/**
 * uprof_report_unref:
 * @report: A #UProfReport object
 *
 * Decreases the reference count of a UProfReport object. When the reference
 * count reaches 0 its associated resources will be freed.
 */
void
uprof_report_unref (UProfReport *report);

/**
 * uprof_report_get_name:
 * @report: A #UProfReport object
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
 * @report: A #UProfReport object
 * @context: a #UProfContext object
 *
 * Associates a context with a report object so that when the report is
 * generated it will include statistics relating to this context.
 */
void
uprof_report_add_context (UProfReport *report,
                          UProfContext *context);

/**
 * UProfReportCallback:
 * @report: A #UProfReport object
 * @user_data: Private user data previously associated with the callback
 *
 * A callback prototype used by uprof_report_set_init_fini_callbacks()
 * which allow you to hook into report generation.
 *
 * Since: 0.4
 */
typedef void (*UProfReportCallback) (UProfReport *report,
                                     gpointer user_data);

/**
 * uprof_report_set_init_fini_callbacks:
 * @report: A #UProfReport object
 * @init: A #UProfReportCallback to be called before report generation
 * @fini: A #UProfReportCallback to be called after report generation
 * @user_data: Private data that will be passed to the @init and @fini
 *             callbacks.
 *
 * Allows you to hook into report generation so you can make sure all the
 * #UProfContext<!-- -->s you need in the report are added and you can
 * specify all the custom attributes you want.
 *
 * You can remove any previously set callbacks by passing %NULL for
 * @init or @fini.
 *
 * Since: 0.4
 */
void
uprof_report_set_init_fini_callbacks (UProfReport *report,
                                      UProfReportCallback init,
                                      UProfReportCallback fini,
                                      gpointer user_data);

typedef void (*UProfReportContextCallback) (UProfReport *report,
                                            UProfContext *context);

void
uprof_report_add_context_callback (UProfReport *report,
                                   UProfReportContextCallback callback);

void
uprof_report_remove_context_callback (UProfReport *report,
                                      UProfReportContextCallback callback);

/**
 * UProfAttributeType:
 * @UPROF_ATTRIBUTE_TYPE_INT: For signed and unsigned integer values.
 * @UPROF_ATTRIBUTE_TYPE_FLOAT: For floating point values.
 * @UPROF_ATTRIBUTE_TYPE_WORD: For a single word string
 * @UPROF_ATTRIBUTE_TYPE_SHORT_STRING: For a single, short line of text.
 * @UPROF_ATTRIBUTE_TYPE_LONG_STRING: For a paragraph or larger blob
 *                                    of text.
 *
 * To allow different user interfaces to be able to provide appropriate
 * formatting, custom attributes they have a type associated with them.
 *
 * The other purpose is to allow tools to convert a string based
 * representation into a numeric representation to perform comparisons
 * e.g. for sorting.
 *
 * Since: 0.4
 */
typedef enum
{
  UPROF_ATTRIBUTE_TYPE_INT,
  UPROF_ATTRIBUTE_TYPE_FLOAT,
  UPROF_ATTRIBUTE_TYPE_WORD,
  UPROF_ATTRIBUTE_TYPE_SHORT_STRING,
  UPROF_ATTRIBUTE_TYPE_LONG_STRING
} UProfAttributeType;

/**
 * uprof_report_add_statistic:
 * @report: A #UProfReport
 * @name: The name of the statistic
 * @name_formatted: The name formatted for the left hand table cell
 *                  entry in a text report (possibly wrapped with
 *                  newline characters)
 * @description: A more detailed explanation of what the statistic shows
 *
 * Adds a custom statistic to generated reports. The statistic must have
 * a unique @name such as "Average FPS" and you can define associated values
 * with that statistic using uprof_report_add_statistic_attribute().
 *
 * A custom statistic can be removed using uprof_report_remove_statistic() and
 * if you give uprof_report_add_statistic() a @name that already exists then
 * the associated @name_formatted and @description will be updated.
 *
 * Since: 0.4
 */
void
uprof_report_add_statistic (UProfReport *report,
                            const char *name,
                            const char *description);

/**
 * uprof_report_remove_statistic:
 * @report: A #UProfReport
 * @name: The name of the statistic
 *
 * Removes any custom statistic with the give @name. If no statistic with the
 * given @name can be found this function does nothing.
 *
 * Since: 0.4
 */
void
uprof_report_remove_statistic (UProfReport *report,
                               const char *name);

/**
 * UProfStatisticAttributeCallback:
 * @report: A #UProfReport
 * @statistic_name: The statistic whos attribute value is being updated.
 * @attribute_name: The attribute whos value needs to be calculated.
 * @user_data: The private data passed to uprof_report_add_statistic_attribute()
 *
 * Is a callback prototype used with uprof_report_add_statistic_attribute()
 * for functions that calculate the value of a statistic attribute
 * when generating a report.
 *
 * Since: 0.4
 */
typedef char *(*UProfStatisticAttributeCallback) (UProfReport *report,
                                                  const char *statistic_name,
                                                  const char *attribute_name,
                                                  gpointer user_data);

/**
 * uprof_report_add_statistic_attribute:
 * @report: A #UProfReport
 * @statistic_name: The name of the statistic which you want to add
 *                  an attribute for.
 * @attribute_name: The name of the attribute
 * @attribute_name_formatted: The name formatted for the column
 *                  header of a text report (possibly wrapped with
 *                  newline characters)
 * @attribute_description: A more detailed explanation of what the
 *                         attribute shows
 * @type: A #UProfAttributeType detailing the attribute's value type.
 * @callback: A #UProfStatisticAttributeCallback to be called when
 *            generating a report to define the current value for
 *            the attribute.
 * @user_data: A pointer to any private data that will be passed to
 *             the given @callback.
 *
 * Adds an attribute to a custom @report statisitic. (See
 * uprof_report_add_statistic()) This will add a new column to a text
 * formatted report. The @name_formatted argument will define the
 * header for the new column and the @callback will be called when
 * generating a report to define the current value. The @attribute_name
 * is always unique for any particular @statistic_name so if you give
 * a pre-existing @attribute_name then that will replace the
 * corresponding @attribute_name_formatted, @attribute_description,
 * @type, @callback and @user_data.
 *
 * Since: 0.4
 */
void
uprof_report_add_statistic_attribute (UProfReport *report,
                                      const char *statistic_name,
                                      const char *attribute_name,
                                      const char *attribute_name_formatted,
                                      const char *attribute_description,
                                      UProfAttributeType type,
                                      UProfStatisticAttributeCallback callback,
                                      gpointer user_data);

/**
 * uprof_report_remove_custom_attribute:
 * @report: A #UProfReport
 * @statistic_name: The name of the statistic whos attribute you want to remove
 * @attribute_name: The name of the attribute
 *
 * Removes the statistic attribute with the given @name from @report. If no
 * attribute with the given @name can be found this function does nothing.
 *
 * Since: 0.4
 */
void
uprof_report_remove_statistic_attribute (UProfReport *report,
                                         const char *statistic_name,
                                         const char *attribute_name);

/**
 * UProfCountersAttributeCallback:
 * @result: The current counter being reported
 * @user_data: The private data previously passed to
 *             uprof_report_add_counters_attribute()
 *
 * Use this prototype for the custom attribute callbacks. The string values
 * that you return in your callback may be wrapped across multiple lines and
 * uprof should still tabulate the report correctly. The
 */
typedef char *(*UProfCountersAttributeCallback) (UProfReport *report,
                                                 UProfCounterResult *result,
                                                 void *user_data);

/**
 * uprof_report_add_counters_attribute:
 * @report: A #UProfReport
 * @name: The name of the attribute
 * @name_formatted: The name formatted for the column header of a
 *                  text report (possibly wrapped with newline
 *                  characters)
 * @description: A more detailed explanation of what the attribute shows
 * @type: A #UProfAttributeType detailing the attributes value type.
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
 * uprof_report_add_counters_attribute (report,
 *                                      "Double count",
 *                                      "Double\ncount",
 *                                      "The count doubled",
 *                                      UPROF_ATTRIBUTE_TYPE_INT,
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
                                     const char *name,
                                     const char *name_formatted,
                                     const char *description,
                                     UProfAttributeType type,
                                     UProfCountersAttributeCallback callback,
                                     void *user_data);

/**
 * uprof_report_remove_counters_attribute:
 * @report: A UProfReport
 * @id: The custom report column you want to remove
 *
 * Removes the custom counters @attribute from future reports generated
 * with uprof_report_print().
 */
void
uprof_report_remove_counters_attribute (UProfReport *report,
                                        const char *attribute_name);

/**
 * UProfTimersAttributeCallback:
 * @result: The current timer being reported
 * @user_data: The private data previously passed to
 *             uprof_report_add_timers_attribute()
 *
 * Use this prototype for the custom attribute callbacks. The string values
 * that you return in your callback may be wrapped across multiple lines and
 * uprof should still tabulate the report correctly. The
 */
typedef char *(*UProfTimersAttributeCallback) (UProfReport *report,
                                               UProfTimerResult *result,
                                               void *user_data);

/**
 * uprof_report_add_timers_attribute:
 * @report: A UProfReport
 * @name: The name of the attribute
 * @name_formatted: The name formatted for the column header of a
 *                  text report (possibly wrapped with newline
 *                  characters)
 * @description: A more detailed explanation of what the attribute shows
 * @type: A #UProfAttributeType detailing the attributes value type.
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
                                   const char *name,
                                   const char *name_formatted,
                                   const char *description,
                                   UProfAttributeType type,
                                   UProfTimersAttributeCallback callback,
                                   void *user_data);

/**
 * uprof_report_remove_timers_attribute:
 * @report: A UProfReport
 * @id: The custom report column you want to remove
 *
 * Removes the custom timers @attribute from future reports generated with
 * uprof_report_print().
 */
void
uprof_report_remove_timers_attribute (UProfReport *report,
                                      const char *attribute_name);

void
uprof_report_print (UProfReport *report);

G_END_DECLS

#endif /* _UPROF_REPORT_H_ */

