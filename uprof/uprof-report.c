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

#include "uprof.h"
#include "uprof-private.h"
#include "uprof-marshal.h"
#include "uprof-timer-result.h"
#include "uprof-timer-result-private.h"
#include "uprof-context-private.h"
#include "uprof-service-private.h"
#include "uprof-report.h"
#include "uprof-report-private.h"
#include "uprof-reportable-glue.h"
#include "uprof-dbus-private.h"

#include <dbus/dbus-glib.h>
#include <glib/gprintf.h>
#include <string.h>

#define UPROF_REPORT_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), UPROF_TYPE_REPORT, UProfReportPrivate))

typedef struct
{
  /* By having a back reference to the report we can pass a
   * UProfReportContextReference pointer as the private data
   * for our trace message callbacks. */
  UProfReport *report;

  UProfContext *context;
  int tracing_enabled;
  int trace_messages_callback_id;
} UProfReportContextReference;

typedef struct _UProfReportRecord
{
  int    height;
  GList *entries;
  void  *data; /* timer / counter */
} UProfReportRecord;

#if 0
typedef enum
{
  UPROF_ALIGNMENT_LEFT,
  UPROF_ALIGNMENT_RIGHT,
  UPROF_ALIGNMENT_CENTER
} UProfAlignment;
#endif

typedef struct _UProfReportEntry
{
  int            width;
  int            height;
#if 0
  UProfAlignment alignment;
#endif
  gboolean       is_percentage;
  float          percentage;
  char         **lines;
} UProfReportEntry;

typedef struct _UProfAttribute
{
  char *name;
  char *name_formatted;
  char *description;
  UProfAttributeType type;
  void *callback;
  void *user_data;
} UProfAttribute;

typedef struct _UProfStatistic
{
  char *name;
  char *description;
  GList *attributes;
} UProfStatistic;

typedef struct _UProfStatisticsGroup
{
  /* All the statistics in a group have the same attribute
   * names/descriptions/types matching the template attributes. */
  GList *template_attributes;

  GList *statistics;

} UProfStatisticsGroup;

struct _UProfReportPrivate
{
  char *name;

  GList *top_contexts;
  GList *context_references;

  UProfReportInitCallback init_callback;
  UProfReportFiniCallback fini_callback;
  void *init_fini_user_data;

  GList *statistics_groups;
  GList *timer_attributes;
  GList *counter_attributes;

  int max_timer_name_size;
};

enum
{
  PROP_0,

  PROP_NAME
};

enum
{
  TRACE_MESSAGE,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (UProfReport, uprof_report, G_TYPE_OBJECT)

GQuark
uprof_report_error_quark (void)
{
  return g_quark_from_static_string ("uprof-report-error-quark");
}

static void
_uprof_report_setup_dbus_reporter_object (UProfReport *report)
{
  UProfReportPrivate *priv = report->priv;
  DBusGConnection *session_bus;
  GError *error = NULL;
  char *obj_name;
  char *obj_path;
  UProfService *service;

  session_bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

  obj_name = _uprof_dbus_canonify_name (g_strdup (priv->name));

  obj_path = g_strdup_printf ("/org/freedesktop/UProf/Reports/%s", obj_name);
  dbus_g_connection_register_g_object (session_bus,
                                       obj_path, G_OBJECT (report));
  g_free (obj_path);

  g_free (obj_name);

  service = _uprof_get_service ();
  _uprof_service_add_report (service, report);
}

void
uprof_report_constructed (GObject *object)
{
  _uprof_report_setup_dbus_reporter_object (UPROF_REPORT (object));
}

static void
_uprof_report_set_name (UProfReport *self, const char *name)
{
  self->priv->name = g_strdup (name);
}

const char *
uprof_report_get_name (UProfReport *self)
{
  return self->priv->name;
}

static void
uprof_report_set_property (GObject      *object,
			   guint         prop_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
  UProfReport *report = UPROF_REPORT (object);

  switch (prop_id)
    {
    case PROP_NAME:
      _uprof_report_set_name (report, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
uprof_report_get_property (GObject    *object,
			   guint       prop_id,
			   GValue     *value,
			   GParamSpec *pspec)
{
  UProfReport *report = UPROF_REPORT (object);
  UProfReportPrivate *priv = report->priv;

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
free_attribute (UProfAttribute *attribute)
{
  g_free (attribute->name);
  g_free (attribute->name_formatted);
  g_free (attribute->description);
  g_slice_free (UProfAttribute, attribute);
}

static void
free_statistic (UProfStatistic *statistic)
{

  g_list_foreach (statistic->attributes, (GFunc)free_attribute, NULL);
  g_slice_free (UProfStatistic, statistic);
}

static void
free_statistics_group (UProfStatisticsGroup *group)
{
  g_list_foreach (group->template_attributes, (GFunc)free_attribute, NULL);
  g_list_foreach (group->statistics, (GFunc)free_statistic, NULL);
  g_slice_free (UProfStatisticsGroup, group);
}

static void
uprof_report_finalize (GObject *object)
{
  UProfReport *report = UPROF_REPORT (object);
  UProfReportPrivate *priv = report->priv;
  UProfService *service;
  GList *contexts;
  GList *l;

  service = _uprof_get_service ();
  _uprof_service_remove_report (service, report);

  g_free (priv->name);

  g_list_foreach (priv->statistics_groups, (GFunc)free_statistics_group, NULL);
  g_list_foreach (priv->timer_attributes, (GFunc)free_attribute, NULL);
  g_list_foreach (priv->counter_attributes, (GFunc)free_attribute, NULL);

  contexts = g_list_copy (priv->top_contexts);
  for (l = priv->top_contexts; l; l = l->next)
    uprof_report_remove_context (report, l->data);
  g_list_free (contexts);

  G_OBJECT_CLASS (uprof_report_parent_class)->finalize (object);
}

static void
uprof_report_class_init (UProfReportClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  object_class->constructed  = uprof_report_constructed;
  object_class->set_property = uprof_report_set_property;
  object_class->get_property = uprof_report_get_property;
  object_class->finalize     = uprof_report_finalize;

  g_type_class_add_private (klass, sizeof (UProfReportPrivate));

  pspec = g_param_spec_string ("name",
                               "Name",
                               "Name of the report",
                               NULL,
                               G_PARAM_WRITABLE | G_PARAM_READABLE |
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME |
                               G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK);
  g_object_class_install_property (object_class, PROP_NAME, pspec);

  signals[TRACE_MESSAGE] =
    g_signal_new ("trace-message",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  _uprof_marshal_VOID__STRING_STRING,
                  G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
}


static void
uprof_report_init (UProfReport *self)
{
  UProfReportPrivate *priv;

  self->priv = priv = UPROF_REPORT_GET_PRIVATE (self);

  priv->top_contexts = NULL;
  priv->context_references = NULL;

  priv->statistics_groups = NULL;
  priv->timer_attributes = NULL;
  priv->counter_attributes = NULL;

  priv->max_timer_name_size = 0;
}

void
_uprof_report_register_dbus_type_info (void)
{
  dbus_g_object_type_install_info (UPROF_TYPE_REPORT,
                                   &dbus_glib__uprof_report_object_info);
}

UProfReport *
uprof_report_new (const char *name)
{
  return g_object_new (UPROF_TYPE_REPORT, "name", name, NULL);
}

UProfReport *
uprof_report_ref (UProfReport *report)
{
  g_object_ref (report);
  return report;
}

void
uprof_report_unref (UProfReport *report)
{
  g_object_unref (report);
}

void
context_trace_message_cb (UProfContext *context,
                          const char *message,
                          void *user_data)
{
  UProfReportContextReference *ref = user_data;
  UProfReport *report = ref->report;

  if (ref->tracing_enabled)
    g_signal_emit (report, signals[TRACE_MESSAGE], 0,
                   context->name,
                   message);
}

static void
add_context_to_list_cb (UProfContext *context,
                        void *user_data)
{
  GList **references = user_data;

  if (!g_list_find (*references, context))
    *references = g_list_prepend (*references, context);
}

static void
update_context_references (UProfReport *report)
{
  UProfReportPrivate *priv = report->priv;
  GList *l;
  GList *contexts = NULL;
  GList *references = NULL;

  /* Get a list of all contexts associated with this report */
  for (l = priv->top_contexts; l; l = l->next)
    _uprof_context_for_self_and_links_recursive (l->data,
                                                 add_context_to_list_cb,
                                                 &contexts);

  /* Move references still needed to the new references list and
   * free the others. (Delete links from contexts as they are
   * accounted for) */
  for (l = priv->context_references; l; l = l->next)
    {
      UProfReportContextReference *ref = l->data;
      GList *link;

      link = g_list_find (contexts, ref->context);
      if (link)
        {
          references = g_list_prepend (references, ref->context);
          contexts = g_list_delete_link (contexts, link);
        }
      else
        {
          int id = ref->trace_messages_callback_id;

          _uprof_context_remove_trace_message_callback (ref->context, id);
          uprof_context_unref (ref->context);

          g_slice_free (UProfReportContextReference, ref);
        }
    }

  /* Create references for newly found contexts */
  for (l = contexts; l; l = l->next)
    {
      UProfContext *context = l->data;
      UProfReportContextReference *ref =
        g_slice_new (UProfReportContextReference);

      ref->context = uprof_context_ref (context);
      /* This back reference is just for the convenience of being able to
       * pass a ref as the private data for our trace message callback... */
      ref->report = report;
      ref->tracing_enabled = 0;
      ref->trace_messages_callback_id =
        _uprof_context_add_trace_message_callback (context,
                                                   context_trace_message_cb,
                                                   ref);

      references = g_list_prepend (references, ref);
    }

  g_list_free (contexts);

  priv->context_references = references;
}

void
uprof_report_add_context (UProfReport *report,
                          UProfContext *context)
{
  UProfReportPrivate *priv = report->priv;
  priv->top_contexts =
    g_list_prepend (priv->top_contexts, uprof_context_ref (context));
  update_context_references (report);
}

void
uprof_report_remove_context (UProfReport *report,
                             UProfContext *context)
{
  UProfReportPrivate *priv = report->priv;

  priv->top_contexts = g_list_remove (priv->top_contexts, context);
  update_context_references (report);
}

void
uprof_report_set_init_fini_callbacks (UProfReport *report,
                                      UProfReportInitCallback init,
                                      UProfReportFiniCallback fini,
                                      gpointer user_data)
{
  UProfReportPrivate *priv = report->priv;
  priv->init_callback = init;
  priv->fini_callback = fini;
  priv->init_fini_user_data = user_data;
}

/* If a matching statistic is found then it's removed from
 * the list of statistic groups since it's assumed that we
 * are about to either remove it or change an attribute. */
static UProfStatistic *
groups_list_remove_statistic (GList **groups, const char *name)
{
  GList *l;

  for (l = *groups; l; l = l->next)
    {
      UProfStatisticsGroup *group = l->data;
      GList *l2;

      for (l2 = group->statistics; l2; l2 = l2->next)
        {
          UProfStatistic *statistic = l2->data;
          if (strcmp (statistic->name, name) == 0)
            {
              group->statistics = g_list_remove_link (group->statistics, l2);

              /* Delete the group if we're removing the last custom
               * statistic from it. */
              if (!group->statistics)
                {
                  *groups = g_list_remove_link (*groups, l);
                  free_statistics_group (group);
                }

              return statistic;
            }
        }
    }

  return NULL;
}

static gboolean
statistic_attributes_equal (GList *attributes0,
                            GList *attributes1)
{
  GList *l;

  for (l = attributes0; l; l = l->next)
    {
      UProfAttribute *attribute0 = l->data;
      GList *l2;

      for (l2 = attributes1; l2; l2 = l2->next)
        {
          UProfAttribute *attribute1 = l2->data;
          if (strcmp (attribute0->name, attribute1->name) == 0 &&
              strcmp (attribute0->name_formatted, attribute1->name_formatted) == 0 &&
              strcmp (attribute0->description, attribute1->description) == 0 &&
              attribute0->type == attribute1->type)
            return TRUE;
        }
    }

  return FALSE;
}

static UProfStatisticsGroup *
find_statistics_group (GList *groups,
                       UProfStatistic *statistic)
{
  GList *l;

  for (l = groups; l; l = l->next)
    {
      UProfStatisticsGroup *group = l->data;
      GList *l2;

      for (l2 = group->statistics; l2; l2 = l2->next)
        {
          if (statistic_attributes_equal (statistic->attributes,
                                          group->template_attributes))
            return group;
        }
    }
  return NULL;
}

static int
compare_attribute_names_cb (gconstpointer a, gconstpointer b)
{
  const UProfAttribute *attribute0 = a;
  const UProfAttribute *attribute1 = b;
  return strcmp (attribute0->name, attribute1->name);
}

static UProfStatisticsGroup *
statistics_group_new (UProfStatistic *template_statistic)
{
  UProfStatisticsGroup *group = g_slice_new0 (UProfStatisticsGroup);
  GList *l;

  for (l = template_statistic->attributes; l; l = l->next)
    {
      UProfAttribute *template_attribute = l->data;

      UProfAttribute *attribute = g_slice_new (UProfAttribute);
      attribute->name = g_strdup (template_attribute->name);
      attribute->name_formatted =
        g_strdup (template_attribute->name_formatted);
      attribute->description = g_strdup (template_attribute->description);
      attribute->type = template_attribute->type;
      /* nothing else needs to be initialized in a template attribute */

      group->template_attributes =
        g_list_insert_sorted (group->template_attributes,
                              attribute,
                              compare_attribute_names_cb);
    }

  return group;
}

int
compare_statistic_names_cb (gconstpointer a, gconstpointer b)
{
  const UProfStatistic *statistic0 = a;
  const UProfStatistic *statistic1 = b;
  return strcmp (statistic0->name, statistic1->name);
}

static GList *
groups_list_add_statistic (GList *groups, UProfStatistic *statistic)
{
  UProfStatisticsGroup *group = find_statistics_group (groups, statistic);

  if (!group)
    {
      group = statistics_group_new (statistic);
      groups = g_list_prepend (groups, group);
    }

  group->statistics = g_list_insert_sorted (group->statistics,
                                            statistic,
                                            compare_statistic_names_cb);

  return groups;
}

static GList *
groups_list_add (GList *groups,
                 const char *name,
                 const char *description)
{
  UProfStatistic *statistic = groups_list_remove_statistic (&groups, name);

  if (statistic)
    {
      g_free (statistic->description);
      statistic->description = g_strdup (description);
      return groups_list_add_statistic (groups, statistic);
    }

  statistic = g_slice_new (UProfStatistic);
  statistic->name = g_strdup (name);
  statistic->description = g_strdup (description);
  statistic->attributes = NULL;

  return groups_list_add_statistic (groups, statistic);
}

void
uprof_report_add_statistic (UProfReport *report,
                            const char *name,
                            const char *description)
{
  report->priv->statistics_groups =
    groups_list_add (report->priv->statistics_groups,
                     name, description);
}

static GList *
groups_list_delete_statistic (GList *groups, const char *name)
{
  UProfStatistic *statistic = groups_list_remove_statistic (&groups, name);

  if (!statistic)
    return groups;

  free_statistic (statistic);

  return groups;
}

void
uprof_report_remove_statistic (UProfReport *report,
                               const char *name)
{
  report->priv->statistics_groups =
    groups_list_delete_statistic (report->priv->statistics_groups, name);
}

static UProfAttribute *
find_attribute (GList *attributes, const char *name)
{
  GList *l;
  for (l = attributes; l; l = l->next)
    {
      UProfAttribute *attribute = l->data;
      if (strcmp (attribute->name, name) == 0)
        return attribute;
    }
  return NULL;
}

static GList *
add_attribute (GList *attributes,
               const char *name,
               const char *name_formatted,
               const char *description,
               UProfAttributeType type,
               void *callback,
               void *user_data)
{
  UProfAttribute *attribute;
  gboolean found = FALSE;

  attribute = find_attribute (attributes, name);
  if (!attribute)
    {
      attribute = g_slice_new (UProfAttribute);
      attribute->name = g_strdup (name);
    }
  else
    {
      g_free (attribute->name_formatted);
      g_free (attribute->description);
      found = TRUE;
    }

  attribute->name_formatted = g_strdup (name_formatted);
  attribute->description = g_strdup (description);
  attribute->type = type;
  attribute->callback = callback;
  attribute->user_data = user_data;

  if (!found)
    return g_list_insert_sorted (attributes,
                                 attribute,
                                 compare_attribute_names_cb);
  else
    return attributes;
}

static GList *
remove_attribute (GList *attributes, const char *name)
{
  GList *l;

  for (l = attributes; l; l = l->next)
    {
      UProfAttribute *attribute = l->data;
      if (strcmp (attribute->name, name) == 0)
        {
          g_free (attribute->name);
          g_slice_free (UProfAttribute, attribute);
          return g_list_delete_link (attributes, l);
        }
    }

  return attributes;
}

static GList *
groups_list_add_statistic_attribute (GList *groups,
                                     const char *statistic_name,
                                     const char *attribute_name,
                                     const char *attribute_name_formatted,
                                     const char *attribute_description,
                                     UProfAttributeType attribute_type,
                                     UProfStatisticAttributeCallback callback,
                                     gpointer user_data)
{
  UProfStatistic *statistic =
    groups_list_remove_statistic (&groups, statistic_name);

  if (!statistic)
    {
      g_warning ("Failed to find a statistic with name \"%s\" when adding"
                 " attribute \"%s\"",
                 statistic_name, attribute_name);
      return groups;
    }

  statistic->attributes =
    add_attribute (statistic->attributes,
                   attribute_name,
                   attribute_name_formatted,
                   attribute_description,
                   attribute_type,
                   callback,
                   user_data);

  return groups_list_add_statistic (groups, statistic);
}

void
uprof_report_add_statistic_attribute (UProfReport *report,
                                      const char *statistic_name,
                                      const char *attribute_name,
                                      const char *attribute_name_formatted,
                                      const char *attribute_description,
                                      UProfAttributeType attribute_type,
                                      UProfStatisticAttributeCallback callback,
                                      gpointer user_data)
{
  report->priv->statistics_groups =
    groups_list_add_statistic_attribute (report->priv->statistics_groups,
                                         statistic_name,
                                         attribute_name,
                                         attribute_name_formatted,
                                         attribute_description,
                                         attribute_type,
                                         callback,
                                         user_data);
}

static GList *
groups_list_remove_statistic_attribute (GList *groups,
                                        const char *statistic_name,
                                        const char *attribute_name)
{
  UProfStatistic *statistic =
    groups_list_remove_statistic (&groups, statistic_name);

  if (!statistic)
    {
      g_warning ("Failed to find a statistic with name \"%s\" when removing"
                 " attribute \"%s\"",
                 statistic_name, attribute_name);
      return groups;
    }

  statistic->attributes =
    remove_attribute (statistic->attributes, attribute_name);

  return groups_list_add_statistic (groups, statistic);
}

void
uprof_report_remove_statistic_attribute (UProfReport *report,
                                         const char *statistic_name,
                                         const char *attribute_name)
{
  report->priv->statistics_groups =
    groups_list_remove_statistic_attribute (report->priv->statistics_groups,
                                            statistic_name,
                                            attribute_name);
}

void
uprof_report_add_counters_attribute (UProfReport *report,
                                     const char *name,
                                     const char *name_formatted,
                                     const char *description,
                                     UProfAttributeType type,
                                     UProfCountersAttributeCallback callback,
                                     void *user_data)
{
  UProfReportPrivate *priv = report->priv;
  priv->counter_attributes =
    add_attribute (priv->counter_attributes,
                   name,
                   name_formatted,
                   description,
                   type,
                   callback,
                   user_data);
}

void
uprof_report_remove_counters_attribute (UProfReport *report,
                                        const char *name)
{
  report->priv->counter_attributes =
    remove_attribute (report->priv->counter_attributes, name);
}

void
uprof_report_add_timers_attribute (UProfReport *report,
                                   const char *name,
                                   const char *name_formatted,
                                   const char *description,
                                   UProfAttributeType type,
                                   UProfTimersAttributeCallback callback,
                                   void *user_data)
{
  UProfReportPrivate *priv = report->priv;
  priv->timer_attributes =
    add_attribute (priv->timer_attributes,
                   name,
                   name_formatted,
                   description,
                   type,
                   callback,
                   user_data);
}

void
uprof_report_remove_timers_attribute (UProfReport *report,
                                      const char *attribute_name)
{
  report->priv->timer_attributes =
    remove_attribute (report->priv->timer_attributes, attribute_name);
}

typedef struct
{
  UProfReport *report;
  GList **records;
} AddCounterState;

static void
add_counter_record (UProfCounterResult *counter,
                    gpointer            data)
{
  AddCounterState        *state = data;
  UProfReport            *report = state->report;
  UProfReportPrivate     *priv = report->priv;
  GList                 **records = state->records;
  UProfReportRecord      *record;
  UProfReportEntry       *entry;
  char                   *lines;
  GList                  *l;

  record = g_slice_new0 (UProfReportRecord);

  entry = g_slice_new0 (UProfReportEntry);
  lines = g_strdup_printf ("%s", uprof_counter_result_get_name (counter));
  entry->lines = g_strsplit (lines, "\n", 0);
  g_free (lines);
  record->entries = g_list_prepend (record->entries, entry);

  entry = g_slice_new0 (UProfReportEntry);
  lines = g_strdup_printf ("%lu", uprof_counter_result_get_count (counter));
  entry->lines = g_strsplit (lines, "\n", 0);
  g_free (lines);
  record->entries = g_list_prepend (record->entries, entry);

  for (l = priv->counter_attributes; l; l = l->next)
    {
      UProfAttribute *attribute = l->data;
      UProfCountersAttributeCallback callback = attribute->callback;
      entry = g_slice_new0 (UProfReportEntry);
      lines = callback (report, counter, attribute->user_data);
      entry->lines = g_strsplit (lines, "\n", 0);
      g_free (lines);
      record->entries = g_list_prepend (record->entries, entry);
    }

  record->entries = g_list_reverse (record->entries);
  record->data = counter;

  *records = g_list_prepend (*records, record);
}

static void
prepare_report_records_for_counters (UProfReport *report,
                                     UProfContext *context,
                                     GList **records)
{
  AddCounterState state;
  state.report = report;
  state.records = records;
  uprof_context_foreach_counter (context,
                                 UPROF_COUNTER_SORT_COUNT_INC,
                                 add_counter_record,
                                 &state);
}

static int
utf8_width (const char *utf8_string)
{
  glong n_chars;
  gunichar *ucs4_string = g_utf8_to_ucs4_fast (utf8_string, -1, &n_chars);
  int i;
  int len = 0;

  for (i = 0; i < n_chars; i++)
    {
      if (g_unichar_iswide (ucs4_string[i]))
        len += 2;
      else if (g_unichar_iszerowidth (ucs4_string[i]))
        continue;
      else
        len++;
    }

  g_free (ucs4_string);

  return len;
}

static void
append_record_entries (GString *buf, UProfReportRecord *record)
{
  int line;
  GList *l;

  for (line = 0; line < record->height; line++)
    {
      for (l = record->entries; l; l = l->next)
        {
          UProfReportEntry *entry = l->data;
          if (entry->height < (line + 1))
            g_string_append_printf (buf, "%-*s ", entry->width, "");
          else
            {
              /* XXX: printf field widths are byte not character oriented
               * so we have to consider multi-byte utf8 characters. */
              int field_width =
                strlen (entry->lines[line]) +
                (entry->width - utf8_width (entry->lines[line]));
              g_string_append_printf (buf, "%-*s ",
                                      field_width, entry->lines[line]);
            }
        }
      g_string_append_printf (buf, "\n");
    }
}

static void
append_counter_record (GString *buf, UProfReportRecord *record)
{
  UProfCounterResult *counter = record->data;

  if (counter && counter->count == 0)
    return;

  append_record_entries (buf, record);

  /* XXX: We currently pass a NULL timer for the first record which contains
   * the column titles... */
  if (counter == NULL)
    g_string_append_printf (buf, "\n");
}

static void
append_counter_records (GString *buf, GList *records)
{
  GList *l;

  for (l = records; l; l = l->next)
    append_counter_record (buf, l->data);
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

/* Note: This returns a bar 45 characters wide for 100% */
char *
get_percentage_bar (float percent)
{
  GString *bar = g_string_new ("");
  int bar_len = 3.6 * percent;
  int bar_char_len = bar_len / 8;
  int i;

  if (bar_len % 8)
    bar_char_len++;

  for (i = bar_len; i >= 8; i -= 8)
    g_string_append_printf (bar, "%s", bars[8]);
  if (i)
    g_string_append_printf (bar, "%s", bars[i]);

  return g_string_free (bar, FALSE);
}

static void
prepare_report_records_for_timer_and_children (UProfReport *report,
                                               UProfTimerResult *timer,
                                               int indent_level,
                                               GList **records)
{
  UProfReportPrivate     *priv = report->priv;
  UProfReportRecord      *record;
  UProfReportEntry       *entry;
  int                     indent;
  char                   *lines;
  UProfTimerResult       *root;
  float                   percent;
  GList                  *l;
  GList                  *children;
  guint64                 timer_total;
  guint64                 root_total;

  record = g_slice_new0 (UProfReportRecord);

  indent = indent_level * 2; /* 2 spaces per indent level */
  entry = g_slice_new0 (UProfReportEntry);
  lines = g_strdup_printf ("%*s%-*s",
                           indent, "",
                           priv->max_timer_name_size + 1 - indent,
                           timer->object.name);
  entry->lines = g_strsplit (lines, "\n", 0);
  g_free (lines);
  record->entries = g_list_prepend (record->entries, entry);

  timer_total = _uprof_timer_result_get_total (timer);
  entry = g_slice_new0 (UProfReportEntry);
  lines = g_strdup_printf ("%-.2f",
                           ((float)timer_total /
                            uprof_get_system_counter_hz()) * 1000.0);
  entry->lines = g_strsplit (lines, "\n", 0);
  g_free (lines);
  record->entries = g_list_prepend (record->entries, entry);

  for (l = priv->timer_attributes; l; l = l->next)
    {
      UProfAttribute *attribute = l->data;
      UProfTimersAttributeCallback callback = attribute->callback;
      entry = g_slice_new0 (UProfReportEntry);
      lines = callback (report, timer, attribute->user_data);
      entry->lines = g_strsplit (lines, "\n", 0);
      g_free (lines);
      record->entries = g_list_prepend (record->entries, entry);
    }

  /* percentages are reported relative to the root timer */
  root = uprof_timer_result_get_root (timer);
  root_total = _uprof_timer_result_get_total (root);
  percent = ((float)timer_total / (float)root_total) * 100.0;

  entry = g_slice_new0 (UProfReportEntry);
  lines = g_strdup_printf ("%7.3f%%", percent);
  entry->lines = g_strsplit (lines, "\n", 0);
  g_free (lines);
  entry->is_percentage = TRUE;
  entry->percentage = percent;
  record->entries = g_list_prepend (record->entries, entry);

  entry = g_slice_new0 (UProfReportEntry);
  lines = get_percentage_bar (percent);
  entry->lines = g_strsplit (lines, "\n", 0);
  g_free (lines);
  record->entries = g_list_prepend (record->entries, entry);

  record->entries = g_list_reverse (record->entries);
  record->data = timer;

  *records = g_list_prepend (*records, record);

  children = _uprof_timer_result_get_children (timer);
  children = g_list_sort_with_data (children,
                                    UPROF_TIMER_SORT_TIME_INC,
                                    NULL);
  for (l = children; l; l = l->next)
    {
      UProfTimerState *child = l->data;

      prepare_report_records_for_timer_and_children (report,
                                                     child,
                                                     indent_level + 1,
                                                     records);
    }
  g_list_free (children);
}

static void
size_record_entries (GList *records)
{
  UProfReportRecord *record;
  int entries_per_record;
  int *column_width;
  GList *l;

  if (!records)
    return;

  /* All records should have the same number of entries */
  record = records->data;
  entries_per_record = g_list_length (record->entries);
  column_width = g_new0 (int, entries_per_record);

  for (l = records; l; l = l->next)
    {
      GList *l2;
      int i;

      record = l->data;

      for (l2 = record->entries, i = 0; l2; l2 = l2->next, i++)
        {
          UProfReportEntry *entry = l2->data;
          int line;

          g_assert (i < entries_per_record);

          for (line = 0; entry->lines[line]; line++)
            entry->width = MAX (entry->width, utf8_width (entry->lines[line]));

          column_width[i] = MAX (column_width[i], entry->width);

          entry->height = line;
          record->height = MAX (record->height, entry->height);
        }
    }

  for (l = records; l; l = l->next)
    {
      GList *l2;
      int i;

      record = l->data;

      for (l2 = record->entries, i = 0; l2; l2 = l2->next, i++)
        {
          UProfReportEntry *entry = l2->data;
          entry->width = column_width[i];
        }
    }

  g_free (column_width);
}

static void
append_timer_record (GString *buf, UProfReportRecord *record)
{
  UProfTimerResult  *timer = record->data;

  if (timer && _uprof_timer_result_get_total (timer) == 0)
    return;

  append_record_entries (buf, record);

  /* XXX: We currently pass a NULL timer for the first record which contains
   * the column titles... */
  if (timer == NULL)
    g_string_append_printf (buf, "\n");
}

static void
append_timer_records (GString *buf, GList *records)
{
  GList *l;

  for (l = records; l; l = l->next)
    append_timer_record (buf, l->data);
}

static void
free_report_records (GList *records)
{
  GList *l;
  for (l = records; l; l = l->next)
    {
      GList *l2;
      UProfReportRecord *record = l->data;
      for (l2 = record->entries; l2; l2 = l2->next)
        {
          UProfReportEntry *entry = l2->data;
          g_strfreev (entry->lines);
          g_slice_free (UProfReportEntry, entry);
        }
      g_slice_free (UProfReportRecord, l->data);
    }
  g_list_free (records);
}

typedef struct
{
#ifdef DEBUG_TIMER_HEIRARACHY
  int indent;
#endif
  UProfTimerState *parent;
  GList *children;
} FindChildrenState;

static void
find_timer_children_of_parent_cb (UProfContext *context,
                                  gpointer user_data)
{
  FindChildrenState *state = user_data;
  GList *l;
  GList *children = NULL;

  for (l = context->timers; l != NULL; l = l->next)
    {
      UProfTimerState *timer = l->data;
      if (timer->parent_name &&
          strcmp (timer->parent_name, state->parent->object.name) == 0)
        children = g_list_prepend (children, timer);
    }

#ifdef DEBUG_TIMER_HEIRARACHY
  for (l = children; l; l = l->next)
    {
      UProfTimerState *timer = l->data;
      g_print ("%*smatch: timer=\"%s\" context=\"%s\"\n",
               state->indent, "",
               timer->object.name, uprof_context_get_name (context));
    }
  state->indent += 2;
#endif

  state->children = g_list_concat (state->children, children);
}

static GList *
find_timer_children (UProfContext *context, UProfTimerState *parent)
{
  FindChildrenState state;

#ifdef DEBUG_TIMER_HEIRARACHY
  GList *l2;
  g_print (" find_timer_children (context = %s, parent = %s):\n",
           uprof_context_get_name (context), parent->object.name);

  state.indent = 0;
#endif

  state.parent = parent;
  state.children = NULL;

  /* NB: links allow users to combine the timers and counters from one
   * context into another, so when searching for the children of any
   * given timer we also need to search any linked contexts... */
  _uprof_context_for_self_and_links_recursive (context,
                                               find_timer_children_of_parent_cb,
                                               &state);

  return state.children;
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
           uprof_context_get_name (context),
           ((UProfObjectState *)timer)->name,
           parent ? ((UProfObjectState *)parent)->name : "NULL");
#endif

  timer->parent = parent;
  timer->children = find_timer_children (context, timer);

#ifdef DEBUG_TIMER_HEIRACHY
  g_print ("resolved children of %s:\n", timer->object.name);
  for (l = timer->children; l != NULL; l = l->next)
    g_print ("  name = %s\n", ((UProfObjectState *)l->data)->name);
#endif

  for (l = timer->children; l != NULL; l = l->next)
    resolve_timer_heirachy (context, (UProfTimerState *)l->data, timer);
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
debug_print_timer_recursive (UProfTimerResult *timer, int indent)
{
  UProfContext *context = uprof_timer_result_get_context (timer);
  GList *l;

  g_print ("%*scontext = %s, timer = %s, parent_name = %s, "
           "parent = %s\n",
           2 * indent, "",
           uprof_context_get_name (context),
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
    debug_print_timer_recursive (l->data, indent);
}

static void
debug_print_timer_heirachy_cb (UProfTimerResult *timer, void *data)
{
  if (timer->parent == NULL)
    debug_print_timer_recursive (timer, 0);
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

  if (context->root_timers)
    g_list_free (context->root_timers);

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
                               debug_print_timer_heirachy_cb,
                               context);
#endif

  context->resolved = TRUE;
}

static void
append_counter_statistics (GString *buf,
                           UProfReport *report,
                           UProfContext *context)
{
  UProfReportPrivate *priv = report->priv;
  GList *records = NULL;
  UProfReportRecord *record;
  UProfReportEntry *entry;
  GList *l;

  g_string_append_printf (buf, "counters:\n");

  record = g_slice_new0 (UProfReportRecord);

  entry = g_slice_new0 (UProfReportEntry);
  entry->lines = g_strsplit ("Name", "\n", 0);
  record->entries = g_list_prepend (record->entries, entry);

  entry = g_slice_new0 (UProfReportEntry);
  entry->lines = g_strsplit ("Total", "\n", 0);
  record->entries = g_list_prepend (record->entries, entry);

  for (l = priv->counter_attributes; l; l = l->next)
    {
      UProfAttribute *attribute = l->data;

      entry = g_slice_new0 (UProfReportEntry);
      entry->lines = g_strsplit (attribute->name, "\n", 0);
      record->entries = g_list_prepend (record->entries, entry);
    }

  record->entries = g_list_reverse (record->entries);
  record->data = NULL;

  records = g_list_prepend (records, record);

  prepare_report_records_for_counters (report, context, &records);

  records = g_list_reverse (records);

  size_record_entries (records);

  append_counter_records (buf, records);

  free_report_records (records);
}

static void
append_timer_statistics (GString *buf,
                         UProfReport *report,
                         UProfContext *context)
{
  UProfReportPrivate *priv = report->priv;
  GList *records = NULL;
  UProfReportRecord *record;
  UProfReportEntry *entry;
  GList *root_timers;
  GList *l;

  g_string_append_printf (buf, "\n");
  g_string_append_printf (buf, "timers:\n");
  g_assert (context->resolved);

  root_timers = uprof_context_get_root_timer_results (context);
  for (l = root_timers; l != NULL; l = l->next)
    {
      UProfTimerResult *timer = l->data;
      GList *l2;

      records = NULL;

      record = g_slice_new0 (UProfReportRecord);

      entry = g_slice_new0 (UProfReportEntry);
      entry->lines = g_strsplit ("Name", "\n", 0);
      record->entries = g_list_prepend (record->entries, entry);

      entry = g_slice_new0 (UProfReportEntry);
      entry->lines = g_strsplit ("Total\nmsecs", "\n", 0);
      record->entries = g_list_prepend (record->entries, entry);

      for (l2 = priv->timer_attributes; l2; l2 = l2->next)
        {
          UProfAttribute *attribute = l2->data;

          entry = g_slice_new0 (UProfReportEntry);
          entry->lines = g_strsplit (attribute->name, "\n", 0);
          record->entries = g_list_prepend (record->entries, entry);
        }

      entry = g_slice_new0 (UProfReportEntry);
      entry->lines = g_strsplit ("Percent", "\n", 0);
      record->entries = g_list_prepend (record->entries, entry);

      /* We need a dummy entry for the last percentage bar column because
       * every record is expected to have the same number of entries. */
      entry = g_slice_new0 (UProfReportEntry);
      entry->lines = g_strsplit ("", "\n", 0);
      record->entries = g_list_prepend (record->entries, entry);

      record->entries = g_list_reverse (record->entries);
      record->data = NULL;

      records = g_list_prepend (records, record);

      prepare_report_records_for_timer_and_children (report, timer,
                                                     0, &records);

      records = g_list_reverse (records);

      size_record_entries (records);

      append_timer_records (buf, records);
      free_report_records (records);
    }

  g_list_free (root_timers);
}

static void
add_statistic_record (UProfReport *report,
                      UProfStatistic *statistic,
                      GList **records)
{
  UProfReportRecord  *record;
  UProfReportEntry   *entry;
  char               *lines;
  GList              *l;

  record = g_slice_new0 (UProfReportRecord);

  entry = g_slice_new0 (UProfReportEntry);
  lines = g_strdup_printf ("%s", statistic->name);
  entry->lines = g_strsplit (lines, "\n", 0);
  g_free (lines);
  record->entries = g_list_prepend (record->entries, entry);

  for (l = statistic->attributes; l; l = l->next)
    {
      UProfAttribute *attribute = l->data;
      UProfStatisticAttributeCallback callback = attribute->callback;
      entry = g_slice_new0 (UProfReportEntry);
      lines = callback (report,
                        statistic->name,
                        attribute->name,
                        attribute->user_data);
      entry->lines = g_strsplit (lines, "\n", 0);
      g_free (lines);
      record->entries = g_list_prepend (record->entries, entry);
    }

  record->entries = g_list_reverse (record->entries);
  record->data = statistic;

  *records = g_list_prepend (*records, record);
}

static void
prepare_report_records_for_statistics_group (UProfReport *report,
                                             UProfStatisticsGroup *group,
                                             GList **records)
{
  GList *l;

  for (l = group->statistics; l; l = l->next)
    add_statistic_record (report, l->data, records);
}

static void
append_statistic_record (GString *buf, UProfReportRecord *record)
{
  append_record_entries (buf, record);

  /* XXX: We currently pass NULL data for the first record which contains
   * the column titles... */
  if (record->data == NULL)
    g_string_append_printf (buf, "\n");
}

static void
append_statistic_records (GString *buf, GList *records)
{
  GList *l;

  for (l = records; l; l = l->next)
    append_statistic_record (buf, l->data);
}

static void
append_statistics_group (GString *buf,
                         UProfReport *report,
                         UProfStatisticsGroup *group)
{
  GList *records = NULL;
  UProfReportRecord *record;
  UProfReportEntry *entry;
  GList *l;

  record = g_slice_new0 (UProfReportRecord);

  entry = g_slice_new0 (UProfReportEntry);
  entry->lines = g_strsplit ("Name", "\n", 0);
  record->entries = g_list_prepend (record->entries, entry);

  /* All statistics in a group have the same attributes */
  for (l = group->template_attributes; l; l = l->next)
    {
      UProfAttribute *attribute = l->data;

      entry = g_slice_new0 (UProfReportEntry);
      entry->lines = g_strsplit (attribute->name_formatted, "\n", 0);
      record->entries = g_list_prepend (record->entries, entry);
    }

  record->entries = g_list_reverse (record->entries);
  record->data = NULL;

  records = g_list_prepend (records, record);

  prepare_report_records_for_statistics_group (report, group, &records);

  records = g_list_reverse (records);

  size_record_entries (records);

  append_statistic_records (buf, records);

  free_report_records (records);
}

static void
append_context_statistics (GString *buf,
                           UProfReport *report,
                           UProfContext *context)
{
  GList *l;

  if (!context->statistics_groups)
    return;

  g_string_append_printf (buf, "custom context statistics:\n");

  for (l = context->statistics_groups; l; l = l->next)
    {
      UProfStatisticsGroup *group = l->data;

      append_statistics_group (buf, report, group);
    }

  g_string_append_printf (buf, "\n");
}

static void
append_context_report (GString *buf,
                       UProfReport *report,
                       UProfContext *context)
{
  g_string_append_printf (buf,
                          "context: %s\n\n",
                          uprof_context_get_name (context));

  append_context_statistics (buf, report, context);

  append_counter_statistics (buf, report, context);

  append_timer_statistics (buf, report, context);
}

static void
append_report_statistics (GString *buf, UProfReport *report)
{
  UProfReportPrivate *priv = report->priv;
  GList *l;

  if (!priv->statistics_groups)
    return;

  g_string_append_printf (buf, "custom report statistics:\n");

  for (l = priv->statistics_groups; l; l = l->next)
    {
      UProfStatisticsGroup *group = l->data;

      append_statistics_group (buf, report, group);
    }

  g_string_append_printf (buf, "\n");
}

static char *
generate_uprof_report (UProfReport *report)
{
  GString *buf = g_string_new ("");
  UProfReportPrivate *priv = report->priv;
  GList *l;
  void *closure;

  for (l = priv->top_contexts; l; l = l->next)
    uprof_context_resolve_timer_heirachy (l->data);

  if (priv->init_callback &&
      !priv->init_callback (report,
                            &closure,
                            priv->init_fini_user_data))
    return NULL;

  append_report_statistics (buf, report);

  for (l = priv->top_contexts; l; l = l->next)
    append_context_report (buf, report, l->data);

  if (priv->fini_callback)
    priv->fini_callback (report,
                         closure,
                         priv->init_fini_user_data);

  return g_string_free (buf, FALSE);
}

gboolean
_uprof_report_get_text_report (UProfReport *report,
                               char **text_ret,
                               GError **error)
{
  *text_ret = generate_uprof_report (report);
  return TRUE;
}

static void
reset_context_cb (UProfContext *context, gpointer user_data)
{
  _uprof_context_reset (context);
}

gboolean
_uprof_report_reset (UProfReport *report, GError **error)
{
  UProfReportPrivate *priv = report->priv;
  GList *l;

  for (l = priv->context_references; l; l = l->next)
    {
      UProfReportContextReference *ref = l->data;
      _uprof_context_for_self_and_links_recursive (ref->context,
                                                   reset_context_cb,
                                                   NULL);
    }

  return TRUE;
}

void
uprof_report_print (UProfReport *report)
{
  char *output = generate_uprof_report (report);
  printf ("%s", output);
  g_free (output);
}

typedef void (*UProfReportMatchingRefCallback) (
                                            UProfReportContextReference *ref,
                                            void *user_data);

/* Either call the callback for one matching context or if context ==
 * NULL call the callback for all contexts linked with this report. */
gboolean
for_matching_context_references (UProfReport *report,
                                 const char *context,
                                 UProfReportMatchingRefCallback callback,
                                 void *user_data)
{
  UProfReportPrivate *priv = report->priv;
  GList *l;
  gboolean found = FALSE;

  for (l = priv->context_references; l; l = l->next)
    {
      UProfReportContextReference *ref = l->data;
      if (context == NULL ||
          strcmp (context, ref->context->name) == 0)
        {
          callback (ref, user_data);

          if (context != NULL)
            found = TRUE;
        }
    }

  if (context != NULL && found == FALSE)
    return FALSE;

  return TRUE;
}

static void
inc_tracing_enabled_cb (UProfReportContextReference *ref,
                        void *user_data)
{
  ref->tracing_enabled++;
}

gboolean
_uprof_report_enable_trace_messages (UProfReport *report,
                                     const char *context,
                                     GError **error)
{
  UProfReportPrivate *priv = report->priv;

  if (strcmp (context, "") == 0)
    context = NULL;

  if (!for_matching_context_references (report,
                                        context,
                                        inc_tracing_enabled_cb,
                                        NULL))
    {
      g_set_error (error,
                   UPROF_REPORT_ERROR,
                   UPROF_REPORT_ERROR_UNKNOWN_CONTEXT,
                   "Could not find a context named \"%s\" associated with "
                   "report \"%s\"",
                   context,
                   priv->name);
      return FALSE;
    }

  return TRUE;
}

static void
dec_tracing_enabled_cb (UProfReportContextReference *ref,
                        void *user_data)
{
  g_return_if_fail (ref->tracing_enabled > 0);
  ref->tracing_enabled--;
}

gboolean
_uprof_report_disable_trace_messages (UProfReport *report,
                                      const char *context,
                                      GError **error)
{
  UProfReportPrivate *priv = report->priv;

  if (strcmp (context, "") == 0)
    context = NULL;

  if (!for_matching_context_references (report,
                                        context,
                                        dec_tracing_enabled_cb,
                                        NULL))
    {
      g_set_error (error,
                   UPROF_REPORT_ERROR,
                   UPROF_REPORT_ERROR_UNKNOWN_CONTEXT,
                   "Could not find a context named \"%s\" associated with "
                   "report \"%s\"",
                   context,
                   priv->name);
      return FALSE;
    }

  return TRUE;
}

static void
append_context_options_cb (UProfReportContextReference *ref,
                           void *user_data)
{
  _uprof_context_append_options_xml (ref->context, user_data);
}

gboolean
_uprof_report_list_options (UProfReport *report,
                            const char *context,
                            char **options,
                            GError **error)
{
  UProfReportPrivate *priv = report->priv;
  GString *options_xml = g_string_new ("<options>\n");

  if (strcmp (context, "") == 0)
    context = NULL;

  if (!for_matching_context_references (report,
                                        context,
                                        append_context_options_cb,
                                        options_xml))
    {
      g_set_error (error,
                   UPROF_REPORT_ERROR,
                   UPROF_REPORT_ERROR_UNKNOWN_CONTEXT,
                   "Could not find a context named \"%s\" associated with "
                   "report \"%s\"",
                   context,
                   priv->name);
      return FALSE;
    }

  g_string_append (options_xml, "</options>\n");
  *options = g_string_free (options_xml, FALSE);

  return TRUE;
}

gboolean
_uprof_report_get_boolean_option (UProfReport *report,
                                  const char *context,
                                  const char *name,
                                  gboolean *value,
                                  GError **error)
{
  UProfReportPrivate *priv = report->priv;
  GList *l;

  if (context == NULL)
    goto error;

  for (l = priv->context_references; l; l = l->next)
    {
      UProfReportContextReference *ref = l->data;
      if (strcmp (ref->context->name, context) == 0)
        {
          if (!_uprof_context_get_boolean_option (ref->context,
                                                  name,
                                                  value,
                                                  error))
            return FALSE;
          else
            return TRUE;
        }
    }

error:
  g_set_error (error,
               UPROF_REPORT_ERROR,
               UPROF_REPORT_ERROR_UNKNOWN_CONTEXT,
               "Unknown context %s", context);
  return FALSE;
}

gboolean
_uprof_report_set_boolean_option (UProfReport *report,
                                  const char *context,
                                  const char *name,
                                  gboolean value,
                                  GError **error)
{
  UProfReportPrivate *priv = report->priv;
  GList *l;

  if (context == NULL)
    goto error;

  for (l = priv->context_references; l; l = l->next)
    {
      UProfReportContextReference *ref = l->data;
      if (strcmp (ref->context->name, context) == 0)
        {
          if (!_uprof_context_set_boolean_option (ref->context,
                                                  name,
                                                  value,
                                                  error))
            return FALSE;
          else
            return TRUE;
        }
    }

error:
  g_set_error (error,
               UPROF_REPORT_ERROR,
               UPROF_REPORT_ERROR_UNKNOWN_CONTEXT,
               "Unknown context %s", context);
  return FALSE;
}

