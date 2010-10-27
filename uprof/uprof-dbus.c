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
#include "uprof-report-proxy.h"
#include "uprof-report-proxy-private.h"

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <glib/gprintf.h>
#include <string.h>

GQuark
uprof_dbus_error_quark (void)
{
  return g_quark_from_static_string ("uprof-dbus-error-quark");
}

static char dbus_obj_name_chars[] =
  "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

char *
_uprof_dbus_canonify_name (char *name)
{
  return g_strcanon (name, dbus_obj_name_chars, '_');
}

static char **
get_all_session_bus_names (GError **error)
{
  DBusGConnection *session_bus;
  DBusGProxy *bus_proxy;
  char **bus_names;

  session_bus = dbus_g_bus_get (DBUS_BUS_SESSION, error);
  if (!session_bus)
    return NULL;

  bus_proxy = dbus_g_proxy_new_for_name (session_bus,
                                         "org.freedesktop.DBus",
                                         "/org/freedesktop/DBus",
                                         "org.freedesktop.DBus");

  if (!dbus_g_proxy_call (bus_proxy, "ListNames", error,
                          G_TYPE_INVALID,
                          G_TYPE_STRV, &bus_names,
                          G_TYPE_INVALID))
    {
      g_object_unref (bus_proxy);
      return NULL;
    }

  g_object_unref (bus_proxy);

  return bus_names;
}

static char **
get_all_uprof_report_names_on_bus (const char *bus_name)
{
  DBusGConnection *session_bus;
  DBusGProxy *proxy;
  GError *error = NULL;
  char **report_names;

  session_bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

  proxy = dbus_g_proxy_new_for_name (session_bus,
                                     bus_name,
                                     "/org/freedesktop/UProf/Service",
                                     "org.freedesktop.UProf.Service");

  if (!dbus_g_proxy_call_with_timeout (proxy,
                                       "ListReports",
                                       100,
                                       &error,
                                       G_TYPE_INVALID,
                                       G_TYPE_STRV, &report_names,
                                       G_TYPE_INVALID))
    {
      g_error_free (error);
      report_names = NULL;
    }

  g_object_unref (proxy);

  return report_names;
}

typedef struct
{
  const char *bus_name;
  char **report_names;
} ReportListEntry;

char **
uprof_dbus_list_reports (GError **error)
{
  char **bus_names;
  char **report_names = NULL;
  GList *all_report_list_entries = NULL;
  int i;
  int j;
  int n_names;
  GList *l;

  bus_names = get_all_session_bus_names (error);
  if (!bus_names)
    return NULL;

  n_names = 0;
  for (i = 0; bus_names[i]; i++)
    {
      ReportListEntry *report_list_entry;
      report_names = get_all_uprof_report_names_on_bus (bus_names[i]);

      if (report_names)
        {
          report_list_entry = g_slice_new (ReportListEntry);
          report_list_entry->bus_name = bus_names[i];
          report_list_entry->report_names = report_names;

          all_report_list_entries =
            g_list_prepend (all_report_list_entries, report_list_entry);

          for (j = 0; report_names[j]; j++)
            n_names++;
        }
    }

  report_names = g_new (char *, n_names + 1);
  report_names[n_names] = NULL;

  i = 0;
  for (l = all_report_list_entries; l; l = l->next)
    {
      ReportListEntry *report_list_entry = l->data;
      for (j = 0; report_list_entry->report_names[j]; j++)
        {
          report_names[i] =
            g_strdup_printf ("%s@%s",
                             report_list_entry->report_names[j],
                             report_list_entry->bus_name);
          i++;
        }
      g_strfreev (report_list_entry->report_names);
      g_slice_free (ReportListEntry, report_list_entry);
    }
  g_list_free (all_report_list_entries);

  g_strfreev (bus_names);

  return report_names;
}

static char *
find_first_bus_with_report (const char *report_name,
                            GError **error)
{
  char **bus_names = get_all_session_bus_names (error);
  char *bus_name = NULL;
  int i;

  if (!bus_names)
    return NULL;

  for (i = 0; bus_names[i]; i++)
    {
      char **report_names = get_all_uprof_report_names_on_bus (bus_names[i]);
      int j;

      if (!report_names)
        continue;

      for (j = 0; report_names[j]; j++)
        if (strcmp (report_names[j], report_name) == 0)
          {
            bus_name = bus_names[i];
            break;
          }

      g_strfreev (report_names);
    }

  g_strfreev (bus_names);

  if (!bus_name)
    {
      g_set_error (error,
                   UPROF_DBUS_ERROR,
                   UPROF_DBUS_ERROR_UNKNOWN_REPORT,
                   "Failed to find the report named \"%s\"",
                   report_name);
    }

  return bus_name;
}

UProfReportProxy *
uprof_dbus_get_report_proxy (const char *report_location,
                             GError **error)
{
  char **strv;
  char *bus_name;
  char *report_name;
  char *name;
  char *report_path;
  DBusGConnection *session_bus;
  DBusGProxy *proxy;
  UProfReportProxy *ret;

  g_return_val_if_fail (report_location != NULL, FALSE);

  strv = g_strsplit (report_location, "@", 0);
  bus_name = strv[1];
  report_name = strv[0];

  g_return_val_if_fail (bus_name != NULL, FALSE);
  g_return_val_if_fail (report_name != NULL, FALSE);

  name = _uprof_dbus_canonify_name (g_strdup (report_name));
  if (!bus_name)
    {
      bus_name = find_first_bus_with_report (name, error);
      g_strfreev (strv);
      return NULL;
    }

  session_bus = dbus_g_bus_get (DBUS_BUS_SESSION, error);
  if (!session_bus)
    {
      g_strfreev (strv);
      return NULL;
    }

  report_path = g_strdup_printf ("/org/freedesktop/UProf/Reports/%s",
                                 name);

  proxy = dbus_g_proxy_new_for_name (session_bus,
                                     bus_name,
                                     report_path,
                                     "org.freedesktop.UProf.Reportable");

  g_free (report_path);

  ret = _uprof_report_proxy_new (bus_name, report_name, proxy);

  g_strfreev (strv);

  return ret;
}

