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

#include "uprof.h"
#include "uprof-private.h"
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

typedef struct _UProfPrintReportEntry
{
  int            width;
  int            height;
#if 0
  UProfAlignment alignment;
#endif
  gboolean       is_percentage;
  float          percentage;
  char         **lines;
} UProfPrintReportEntry;

typedef struct _UProfTimerAttribute
{
  char *name;
  UProfReportTimersAttributeCallback callback;
  void *user_data;
} UProfTimerAttribute;

typedef struct _UProfCounterAttribute
{
  char *name;
  UProfReportCountersAttributeCallback callback;
  void *user_data;
} UProfCounterAttribute;

struct _UProfReportPrivate
{
  char *name;

  GList *contexts;
  GList *context_callbacks;

  GList *timer_attributes;
  GList *counter_attributes;

  int max_timer_name_size;
};

enum
{
  PROP_0,

  PROP_NAME
};

G_DEFINE_TYPE (UProfReport, uprof_report, G_TYPE_OBJECT)

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
uprof_report_finalize (GObject *object)
{
  UProfReport *report = UPROF_REPORT (object);
  UProfReportPrivate *priv = report->priv;
  UProfService *service;

  service = _uprof_get_service ();
  _uprof_service_remove_report (service, report);

  g_free (priv->name);

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
}


static void
uprof_report_init (UProfReport *self)
{
  UProfReportPrivate *priv;

  self->priv = priv = UPROF_REPORT_GET_PRIVATE (self);

  priv->contexts = NULL;

  priv->context_callbacks = NULL;

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
uprof_report_add_context (UProfReport *report,
                          UProfContext *context)
{
  UProfReportPrivate *priv = report->priv;
  priv->contexts = g_list_prepend (priv->contexts, context);
}

void
uprof_report_add_context_callback (UProfReport *report,
                                   UProfReportContextCallback callback)
{
  UProfReportPrivate *priv = report->priv;
  priv->context_callbacks =
    g_list_prepend (priv->context_callbacks, callback);
}

void
uprof_report_remove_context_callback (UProfReport *report,
                                      UProfReportContextCallback callback)
{
  UProfReportPrivate *priv = report->priv;
  priv->context_callbacks =
    g_list_remove (priv->context_callbacks, callback);
}

void
uprof_report_add_counters_attribute (UProfReport *report,
                                     const char *attribute_name,
                                     UProfReportCountersAttributeCallback callback,
                                     void *user_data)
{
  UProfReportPrivate *priv = report->priv;
  UProfCounterAttribute *attribute = g_slice_new (UProfCounterAttribute);
  attribute->name = g_strdup (attribute_name);
  attribute->callback = callback;
  attribute->user_data = user_data;
  priv->counter_attributes =
    g_list_append (priv->counter_attributes, attribute);
}

void
uprof_report_remove_counters_attribute (UProfReport *report,
                                        const char *attribute_name)
{
  UProfReportPrivate *priv = report->priv;
  GList *l;

  for (l = priv->counter_attributes; l; l = l->next)
    {
      UProfCounterAttribute *attribute = l->data;
      if (strcmp (attribute->name, attribute_name) == 0)
        {
          g_free (attribute->name);
          g_slice_free (UProfCounterAttribute, attribute);
          priv->counter_attributes =
            g_list_delete_link (priv->counter_attributes, l);
          return;
        }
    }
}

void
uprof_report_add_timers_attribute (UProfReport *report,
                                   const char *attribute_name,
                                   UProfReportTimersAttributeCallback callback,
                                   void *user_data)
{
  UProfReportPrivate *priv = report->priv;
  UProfTimerAttribute *attribute = g_slice_new (UProfTimerAttribute);
  attribute->name = g_strdup (attribute_name);
  attribute->callback = callback;
  attribute->user_data = user_data;
  priv->timer_attributes =
    g_list_append (priv->timer_attributes, attribute);
}

void
uprof_report_remove_timers_attribute (UProfReport *report,
                                      const char *attribute_name)
{
  UProfReportPrivate *priv = report->priv;
  GList *l;

  for (l = priv->timer_attributes; l; l = l->next)
    {
      UProfTimerAttribute *attribute = l->data;
      if (strcmp (attribute->name, attribute_name) == 0)
        {
          g_free (attribute->name);
          g_slice_free (UProfTimerAttribute, attribute);
          priv->timer_attributes =
            g_list_delete_link (priv->timer_attributes, l);
          return;
        }
    }
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
  UProfPrintReportEntry  *entry;
  char                   *lines;
  GList                  *l;

  record = g_slice_new0 (UProfReportRecord);

  entry = g_slice_new0 (UProfPrintReportEntry);
  lines = g_strdup_printf ("%s", uprof_counter_result_get_name (counter));
  entry->lines = g_strsplit (lines, "\n", 0);
  g_free (lines);
  record->entries = g_list_prepend (record->entries, entry);

  entry = g_slice_new0 (UProfPrintReportEntry);
  lines = g_strdup_printf ("%lu", uprof_counter_result_get_count (counter));
  entry->lines = g_strsplit (lines, "\n", 0);
  g_free (lines);
  record->entries = g_list_prepend (record->entries, entry);

  for (l = priv->counter_attributes; l; l = l->next)
    {
      UProfCounterAttribute *attribute = l->data;
      entry = g_slice_new0 (UProfPrintReportEntry);
      lines = attribute->callback (counter, attribute->user_data);
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
get_record_entries_as_text (GString *buf, UProfReportRecord *record)
{
  int line;
  GList *l;

  for (line = 0; line < record->height; line++)
    {
      for (l = record->entries; l; l = l->next)
        {
          UProfPrintReportEntry *entry = l->data;
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
get_counter_record_as_text (GString *buf, UProfReportRecord *record)
{
  UProfCounterResult *counter = record->data;

  if (counter && counter->count == 0)
    return;

  get_record_entries_as_text (buf, record);

  /* XXX: We currently pass a NULL timer for the first record which contains
   * the column titles... */
  if (counter == NULL)
    g_string_append_printf (buf, "\n");
}

static void
get_counter_records_as_text (GString *buf, GList *records)
{
  GList *l;

  for (l = records; l; l = l->next)
    get_counter_record_as_text (buf, l->data);
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
  UProfPrintReportEntry  *entry;
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
  entry = g_slice_new0 (UProfPrintReportEntry);
  lines = g_strdup_printf ("%*s%-*s",
                           indent, "",
                           priv->max_timer_name_size + 1 - indent,
                           timer->object.name);
  entry->lines = g_strsplit (lines, "\n", 0);
  g_free (lines);
  record->entries = g_list_prepend (record->entries, entry);

  timer_total = _uprof_timer_result_get_total (timer);
  entry = g_slice_new0 (UProfPrintReportEntry);
  lines = g_strdup_printf ("%-.2f",
                           ((float)timer_total /
                            uprof_get_system_counter_hz()) * 1000.0);
  entry->lines = g_strsplit (lines, "\n", 0);
  g_free (lines);
  record->entries = g_list_prepend (record->entries, entry);

  for (l = priv->timer_attributes; l; l = l->next)
    {
      UProfTimerAttribute *attribute = l->data;
      entry = g_slice_new0 (UProfPrintReportEntry);
      lines = attribute->callback (timer, attribute->user_data);
      entry->lines = g_strsplit (lines, "\n", 0);
      g_free (lines);
      record->entries = g_list_prepend (record->entries, entry);
    }

  /* percentages are reported relative to the root timer */
  root = uprof_timer_result_get_root (timer);
  root_total = _uprof_timer_result_get_total (root);
  percent = ((float)timer_total / (float)root_total) * 100.0;

  entry = g_slice_new0 (UProfPrintReportEntry);
  lines = g_strdup_printf ("%7.3f%%", percent);
  entry->lines = g_strsplit (lines, "\n", 0);
  g_free (lines);
  entry->is_percentage = TRUE;
  entry->percentage = percent;
  record->entries = g_list_prepend (record->entries, entry);

  entry = g_slice_new0 (UProfPrintReportEntry);
  lines = get_percentage_bar (percent);
  entry->lines = g_strsplit (lines, "\n", 0);
  g_free (lines);
  record->entries = g_list_prepend (record->entries, entry);

  record->entries = g_list_reverse (record->entries);
  record->data = timer;

  *records = g_list_prepend (*records, record);

  children = uprof_timer_result_get_children (timer);
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
          UProfPrintReportEntry *entry = l2->data;
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
          UProfPrintReportEntry *entry = l2->data;
          entry->width = column_width[i];
        }
    }

  g_free (column_width);
}

static void
get_timer_record_as_text (GString *buf, UProfReportRecord *record)
{
  UProfTimerResult  *timer = record->data;

  if (timer && timer->count == 0)
    return;

  get_record_entries_as_text (buf, record);

  /* XXX: We currently pass a NULL timer for the first record which contains
   * the column titles... */
  if (timer == NULL)
    g_string_append_printf (buf, "\n");
}

static void
get_timer_records_as_text (GString *buf, GList *records)
{
  GList *l;

  for (l = records; l; l = l->next)
    get_timer_record_as_text (buf, l->data);
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
          UProfPrintReportEntry *entry = l2->data;
          g_strfreev (entry->lines);
          g_slice_free (UProfPrintReportEntry, entry);
        }
      g_slice_free (UProfReportRecord, l->data);
    }
  g_list_free (records);
}

static void
get_context_report_as_text (GString *buf,
                            UProfReport *report,
                            UProfContext *context)
{
  UProfReportPrivate *priv = report->priv;
  GList *records = NULL;
  UProfReportRecord *record;
  UProfPrintReportEntry *entry;
  GList *root_timers;
  GList *l;

  g_string_append_printf (buf,
                          "context: %s\n"
                          "\n"
                          "counters:\n",
                          uprof_context_get_name (context));

  record = g_slice_new0 (UProfReportRecord);

  entry = g_slice_new0 (UProfPrintReportEntry);
  entry->lines = g_strsplit ("Name", "\n", 0);
  record->entries = g_list_prepend (record->entries, entry);

  entry = g_slice_new0 (UProfPrintReportEntry);
  entry->lines = g_strsplit ("Total", "\n", 0);
  record->entries = g_list_prepend (record->entries, entry);

  for (l = priv->counter_attributes; l; l = l->next)
    {
      UProfCounterAttribute *attribute = l->data;

      entry = g_slice_new0 (UProfPrintReportEntry);
      entry->lines = g_strsplit (attribute->name, "\n", 0);
      record->entries = g_list_prepend (record->entries, entry);
    }

  record->entries = g_list_reverse (record->entries);
  record->data = NULL;

  records = g_list_prepend (records, record);

  prepare_report_records_for_counters (report, context, &records);

  records = g_list_reverse (records);

  size_record_entries (records);

  get_counter_records_as_text (buf, records);

  free_report_records (records);

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

      entry = g_slice_new0 (UProfPrintReportEntry);
      entry->lines = g_strsplit ("Name", "\n", 0);
      record->entries = g_list_prepend (record->entries, entry);

      entry = g_slice_new0 (UProfPrintReportEntry);
      entry->lines = g_strsplit ("Total\nmsecs", "\n", 0);
      record->entries = g_list_prepend (record->entries, entry);

      for (l2 = priv->timer_attributes; l2; l2 = l2->next)
        {
          UProfTimerAttribute *attribute = l2->data;

          entry = g_slice_new0 (UProfPrintReportEntry);
          entry->lines = g_strsplit (attribute->name, "\n", 0);
          record->entries = g_list_prepend (record->entries, entry);
        }

      entry = g_slice_new0 (UProfPrintReportEntry);
      entry->lines = g_strsplit ("Percent", "\n", 0);
      record->entries = g_list_prepend (record->entries, entry);

      /* We need a dummy entry for the last percentage bar column because
       * every record is expected to have the same number of entries. */
      entry = g_slice_new0 (UProfPrintReportEntry);
      entry->lines = g_strsplit ("", "\n", 0);
      record->entries = g_list_prepend (record->entries, entry);

      record->entries = g_list_reverse (record->entries);
      record->data = NULL;

      records = g_list_prepend (records, record);

      prepare_report_records_for_timer_and_children (report, timer,
                                                     0, &records);

      records = g_list_reverse (records);

      size_record_entries (records);

      get_timer_records_as_text (buf, records);
      free_report_records (records);
    }
  g_list_free (root_timers);
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
               timer->object.name, context->name);
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
           context->name, parent->object.name);

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
           context->name,
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
                               debug_print_timer_heirachy_cb,
                               context);
#endif

  context->resolved = TRUE;
}

static void
report_context (GString *buf, UProfReport *report, UProfContext *context)
{
  UProfReportPrivate *priv = report->priv;
  GList *l;

  uprof_context_resolve_timer_heirachy (context);

  /* XXX: callbacks should be considered deprecated since they disable
   * the normal report generation and don't support appending to a
   * GString. */
  if (priv->context_callbacks)
    {
      for (l = priv->context_callbacks; l != NULL; l = l->next)
        {
          UProfReportContextCallback callback = l->data;
          callback (report, context);
        }
      return;
    }

  get_context_report_as_text (buf, report, context);
}

static void
generate_uprof_report (GString *buf, UProfReport *report)
{
  UProfReportPrivate *priv = report->priv;
  GList *l;

  for (l = priv->contexts; l; l = l->next)
    report_context (buf, report, l->data);
}

gboolean
_uprof_report_get_text_report (UProfReport *report,
                               char **text_ret,
                               GError **error)
{
  GString *buf = g_string_new ("");

  generate_uprof_report (buf, report);
  *text_ret = buf->str;
  g_string_free (buf, FALSE);

  return TRUE;
}

void
uprof_report_print (UProfReport *report)
{
  GString *buf = g_string_new ("");

  generate_uprof_report (buf, report);
  printf ("%s", buf->str);
  g_string_free (buf, TRUE);
}

void
uprof_print_percentage_bar (float percent)
{
  char *percentage_bar = get_percentage_bar (percent);

  g_print ("%s", percentage_bar);
  g_free (percentage_bar);
}

/* XXX: Only used to support deprecated API */
static void
print_timer_and_children (UProfReport                   *report,
                          UProfTimerResult              *timer,
                          UProfTimerResultPrintCallback  callback,
                          int                            indent_level,
                          gpointer                       data)
{
  UProfReportPrivate *priv = report->priv;
  char               *extra_fields;
  guint               extra_fields_width = 0;
  UProfTimerResult   *root;
  GList              *children;
  GList              *l;
  float               percent;
  int                 indent;
  guint64             timer_total;
  guint64             root_total;

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

  timer_total = _uprof_timer_result_get_total (timer);
  root_total = _uprof_timer_result_get_total (root);

  percent = ((float)timer_total / (float)root_total) * 100.0;
  g_print (" %*s%-*s%-10.2f  %*s %7.3f%% ",
           indent,
           "",
           priv->max_timer_name_size + 1 - indent,
           timer->object.name,
           ((float)timer_total / uprof_get_system_counter_hz()) * 1000.0,
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

static int
get_name_size_for_timer_and_children (UProfTimerResult *timer,
                                      int indent_level)
{
  int max_name_size =
    (indent_level * 2) + utf8_width (uprof_timer_result_get_name (timer));
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

/* XXX: Deprecated API */
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
  report->priv->max_timer_name_size =
    get_name_size_for_timer_and_children (timer, 0);

  g_print (" %-*s%s %*s %s\n",
           report->priv->max_timer_name_size + 1,
           "Name",
           "Total msecs",
           extra_titles_width,
           extra_titles,
           "Percent");

  g_free (extra_titles);

  print_timer_and_children (report, timer, callback, 0, data);
  g_free (report);
}


