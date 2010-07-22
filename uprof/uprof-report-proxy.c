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
#include "uprof-dbus.h"
#include "uprof-dbus-private.h"
#include "uprof-report-proxy.h"
#include "uprof-report-proxy-private.h"

#include <dbus/dbus-glib.h>
#include <glib/gprintf.h>
#include <string.h>

typedef struct
{
  int id;
  char *context;
  UProfReportProxyTraceMessageFilter filter;
  void *user_data;
} UProfReportProxyTraceMessageFilterData;

static void
on_destroy (DBusGProxy *dbus_g_proxy, void *user_data)
{
  UProfReportProxy *proxy = user_data;

  proxy->destroyed = TRUE;
}

static void
trace_message_cb (DBusGProxy *dbus_g_proxy,
                  const char *context,
                  const char *message,
                  void *user_data)
{
  UProfReportProxy *proxy = user_data;
  GList *l;

  for (l = proxy->trace_message_filters; l; l = l->next)
    {
      UProfReportProxyTraceMessageFilterData *data = l->data;
      char *categories_start = strchr (message, '[');
      char *categories_end = strchr (categories_start, ']');
      char *location_end = strchr (categories_end, '&');

      if (!categories_start)
        {
          g_warning ("Failed to parse trace message: %s",
                     message);
          continue;
        }
      *categories_end = '\0';

      if (!location_end)
        {
          g_warning ("Failed to parse trace message: %s",
                     message);
          continue;
        }
      *location_end = '\0';

      data->filter (proxy,
                    context,
                    categories_start + 1, /* "category0,category1\0" */
                    categories_end + 1, /* " filename:line:function\0" */
                    location_end + 1,
                    data->user_data);
    }
}

UProfReportProxy *
_uprof_report_proxy_new (const char *bus_name,
                         const char *report_name,
                         DBusGProxy *dbus_g_proxy)
{
  UProfReportProxy *proxy = g_slice_new0 (UProfReportProxy);

  proxy->bus_name = g_strdup (bus_name);
  proxy->report_name = g_strdup (report_name);
  proxy->dbus_g_proxy = dbus_g_proxy;

  g_signal_connect (dbus_g_proxy, "destroy", (GCallback)on_destroy, proxy);

  dbus_g_proxy_add_signal (dbus_g_proxy, "TraceMessage",
                           G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);

  dbus_g_proxy_connect_signal (dbus_g_proxy, "TraceMessage",
                               G_CALLBACK (trace_message_cb),
                               proxy, NULL);

  return proxy;
}

UProfReportProxy *
uprof_report_proxy_ref (UProfReportProxy *proxy)
{
  proxy->ref++;
  return proxy;
}

void
uprof_report_proxy_unref (UProfReportProxy *proxy)
{
  proxy->ref--;
  if (!proxy->ref)
    {
      g_free (proxy->bus_name);
      g_free (proxy->report_name);
      g_slice_free (UProfReportProxy, proxy);
    }
}

static gboolean
lost_connection (UProfReportProxy *proxy, GError **error)
{
  if (proxy->destroyed)
    {
      g_set_error (error,
                   UPROF_DBUS_ERROR,
                   UPROF_DBUS_ERROR_DISCONNECTED,
                   "Lost connection to UProf reportable object");
      return TRUE;
    }

  return FALSE;
}

char *
uprof_report_proxy_get_text_report (UProfReportProxy *proxy,
                                    GError **error)
{
  char *text_report;

  if (lost_connection (proxy, error))
    return NULL;

  if (!dbus_g_proxy_call_with_timeout (proxy->dbus_g_proxy,
                                       "GetTextReport",
                                       1000,
                                       error,
                                       G_TYPE_INVALID,
                                       G_TYPE_STRING, &text_report,
                                       G_TYPE_INVALID))
    text_report = NULL;

  return text_report;
}

gboolean
uprof_report_proxy_reset (UProfReportProxy *proxy,
                          GError **error)
{
  if (lost_connection (proxy, error))
    return FALSE;

  if (!dbus_g_proxy_call_with_timeout (proxy->dbus_g_proxy,
                                       "Reset",
                                       1000,
                                       error,
                                       G_TYPE_INVALID,
                                       G_TYPE_INVALID))
    return FALSE;

  return TRUE;
}

int
uprof_report_proxy_add_trace_message_filter (
                                     UProfReportProxy *proxy,
                                     const char *context,
                                     UProfReportProxyTraceMessageFilter filter,
                                     void *user_data,
                                     GError **error)
{
  UProfReportProxyTraceMessageFilterData *data;

  if (proxy->trace_message_filters == NULL)
    {
      /* NULL matches all contexts so we use that for simplicity */
      char *context_name = NULL;

      if (lost_connection (proxy, error))
        return 0;

      if (!dbus_g_proxy_call_with_timeout (proxy->dbus_g_proxy,
                                           "EnableTraceMessages",
                                           1000,
                                           error,
                                           G_TYPE_STRING, context_name,
                                           G_TYPE_INVALID,
                                           G_TYPE_INVALID))
        return 0;
    }

  data = g_slice_new (UProfReportProxyTraceMessageFilterData);

  data->id = ++proxy->next_trace_message_filter_id;
  data->context = g_strdup (context);
  data->filter = filter;
  data->user_data = user_data;

  proxy->trace_message_filters =
    g_list_prepend (proxy->trace_message_filters, data);

  return data->id;
}

static UProfReportProxyTraceMessageFilterData *
remove_trace_message_filter_data (UProfReportProxy *proxy, int id)
{
  GList *l;

  for (l = proxy->trace_message_filters; l; l = l->next)
    {
      UProfReportProxyTraceMessageFilterData *data = l->data;
      if (data->id == id)
        {
          proxy->trace_message_filters =
            g_list_delete_link (proxy->trace_message_filters, l);
          return data;
        }
    }
  return NULL;
}

gboolean
uprof_report_proxy_remove_trace_message_filter (UProfReportProxy *proxy,
                                                int id,
                                                GError **error)
{
  UProfReportProxyTraceMessageFilterData *data =
    remove_trace_message_filter_data (proxy, id);

  g_return_val_if_fail (data != NULL, TRUE);

  g_free (data->context);
  g_slice_free (UProfReportProxyTraceMessageFilterData, data);

  if (proxy->trace_message_filters == NULL)
    {
      /* NULL matches all contexts so we use that for simplicity */
      char *context_name = NULL;

      if (lost_connection (proxy, error))
        return FALSE;

      if (!dbus_g_proxy_call_with_timeout (proxy->dbus_g_proxy,
                                           "DisableTraceMessages",
                                           1000,
                                           error,
                                           G_TYPE_STRING, context_name,
                                           G_TYPE_INVALID,
                                           G_TYPE_INVALID))
        return FALSE;
    }

  return TRUE;
}

void
option_tag_start_cb (GMarkupParseContext *context,
                     const char *element_name,
                     const char **attribute_names,
                     const char **attribute_values,
                     void *user_data,
                     GError **error)
{
  GList **options = user_data;
  UProfReportProxyOption *option;
  int i;

  if (strcmp (element_name, "option") != 0)
    return;

  option = g_slice_new (UProfReportProxyOption);

  for (i = 0; attribute_names[i]; i++)
    {
      if (strcmp (attribute_names[i], "context") == 0)
        option->context = g_strdup (attribute_values[i]);
      else if (strcmp (attribute_names[i], "type") == 0)
        {
          if (strcmp (attribute_values[i], "boolean") != 0)
            goto error;
        }
      else if (strcmp (attribute_names[i], "group") == 0)
        option->group = g_strdup (attribute_values[i]);
      else if (strcmp (attribute_names[i], "name") == 0)
        option->name = g_strdup (attribute_values[i]);
      else if (strcmp (attribute_names[i], "name_formatted") == 0)
        option->name_formatted = g_strdup (attribute_values[i]);
      else if (strcmp (attribute_names[i], "description") == 0)
        option->description = g_strdup (attribute_values[i]);
    }

  *options = g_list_prepend (*options, option);

  return;
error:
  g_slice_free (UProfReportProxyOption, option);
  g_set_error (error,
               G_MARKUP_ERROR,
               G_MARKUP_ERROR_INVALID_CONTENT,
               "Failed to parse an options list");
}

static void
option_tags_error_cb (GMarkupParseContext *context,
                      GError *error,
                      void *user_data)
{

}

gboolean
uprof_report_proxy_foreach_option (UProfReportProxy *proxy,
                                   const char *context,
                                   UProfReportProxyOptionCallback callback,
                                   void *user_data,
                                   GError **error)
{
  char *options_xml;
  GMarkupParser parser;
  GMarkupParseContext *markup_context;
  GList *options = NULL;
  GList *l;
  gboolean cont;

  if (lost_connection (proxy, error))
    return FALSE;

  if (!dbus_g_proxy_call_with_timeout (proxy->dbus_g_proxy,
                                       "ListOptions",
                                       1000,
                                       error,
                                       G_TYPE_STRING, context,
                                       G_TYPE_INVALID,
                                       G_TYPE_STRING, &options_xml,
                                       G_TYPE_INVALID))
    return FALSE;

  parser.start_element = option_tag_start_cb;
  parser.end_element = NULL;
  parser.text = NULL;
  parser.passthrough = NULL;
  parser.error = option_tags_error_cb;

  markup_context = g_markup_parse_context_new (&parser, 0, &options, NULL);
  if (!g_markup_parse_context_parse (markup_context,
                                     options_xml,
                                     strlen (options_xml),
                                     error))
    return FALSE;

  for (l = options, cont = TRUE;
       l && cont;
       l = l->next)
    {
      UProfReportProxyOption *option = l->data;

      cont = callback (proxy, context, option, user_data);
      g_free (option->context);
      g_free (option->name);
      g_free (option->name_formatted);
      g_free (option->description);
      g_slice_free (UProfReportProxyOption, option);
    }
  g_list_free (options);

  g_markup_parse_context_free (markup_context);

  g_free (options_xml);

  return TRUE;
}

gboolean
uprof_report_proxy_get_boolean_option (UProfReportProxy *proxy,
                                       const char *context,
                                       const char *name,
                                       gboolean *value,
                                       GError **error)
{
  if (lost_connection (proxy, error))
    return FALSE;

  if (!dbus_g_proxy_call_with_timeout (proxy->dbus_g_proxy,
                                       "GetBooleanOption",
                                       1000,
                                       error,
                                       G_TYPE_STRING, context,
                                       G_TYPE_STRING, name,
                                       G_TYPE_INVALID,
                                       G_TYPE_BOOLEAN, value,
                                       G_TYPE_INVALID))
    return FALSE;

  return TRUE;
}

gboolean
uprof_report_proxy_set_boolean_option (UProfReportProxy *proxy,
                                       const char *context,
                                       const char *name,
                                       gboolean value,
                                       GError **error)
{
  if (lost_connection (proxy, error))
    return FALSE;

  if (!dbus_g_proxy_call_with_timeout (proxy->dbus_g_proxy,
                                       "SetBooleanOption",
                                       1000,
                                       error,
                                       G_TYPE_STRING, context,
                                       G_TYPE_STRING, name,
                                       G_TYPE_BOOLEAN, value,
                                       G_TYPE_INVALID,
                                       G_TYPE_INVALID))
    return FALSE;

  return TRUE;
}

