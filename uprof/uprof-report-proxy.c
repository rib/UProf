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

static void
on_destroy (DBusGProxy *dbus_g_proxy, void *user_data)
{
  UProfReportProxy *proxy = user_data;

  proxy->destroyed = TRUE;
}

UProfReportProxy *
_uprof_report_proxy_new (const char *bus_name,
                         const char *report_name,
                         DBusGProxy *dbus_g_proxy)
{
  UProfReportProxy *proxy = g_slice_new (UProfReportProxy);

  proxy->bus_name = g_strdup (bus_name);
  proxy->report_name = g_strdup (report_name);
  proxy->dbus_g_proxy = dbus_g_proxy;

  g_signal_connect (dbus_g_proxy, "destroy", (GCallback)on_destroy, proxy);

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

gboolean
uprof_report_proxy_enable_trace_messages (UProfReportProxy *proxy,
                                          const char *context_name,
                                          GError **error)
{
  if (lost_connection (proxy, error))
    return FALSE;

  if (!dbus_g_proxy_call_with_timeout (proxy->dbus_g_proxy,
                                       "EnableTraceMessages",
                                       1000,
                                       error,
                                       G_TYPE_INVALID,
                                       G_TYPE_STRING, &context_name,
                                       G_TYPE_INVALID))
    return FALSE;

  return TRUE;
}

gboolean
uprof_report_proxy_disable_trace_messages (UProfReportProxy *proxy,
                                           const char *context_name,
                                           GError **error)
{
  if (lost_connection (proxy, error))
    return FALSE;

  if (!dbus_g_proxy_call_with_timeout (proxy->dbus_g_proxy,
                                       "DisableTraceMessages",
                                       1000,
                                       error,
                                       G_TYPE_INVALID,
                                       G_TYPE_STRING, &context_name,
                                       G_TYPE_INVALID))
    return FALSE;

  return TRUE;
}

