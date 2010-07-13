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
#include "uprof-report.h"
#include "uprof-report-private.h"

#include <dbus/dbus-glib.h>
#include <glib/gprintf.h>
#include <string.h>

static char dbus_obj_name_chars[] =
  "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

char *
_uprof_dbus_canonify_name (char *name)
{
  return g_strcanon (name, dbus_obj_name_chars, '_');
}

static char **
get_all_session_bus_names (void)
{
  DBusGConnection *session_bus;
  DBusGProxy *bus_proxy;
  GError *error = NULL;
  char **bus_names;

  session_bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

  bus_proxy = dbus_g_proxy_new_for_name (session_bus,
                                         "org.freedesktop.DBus",
                                         "/org/freedesktop/DBus",
                                         "org.freedesktop.DBus");

  if (!dbus_g_proxy_call (bus_proxy, "ListNames", &error,
                          G_TYPE_INVALID,
                          G_TYPE_STRV, &bus_names,
                          G_TYPE_INVALID))
    {
      g_warning ("Failed to ListNames: %s", error->message);
      g_error_free (error);
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
uprof_dbus_list_reports (void)
{
  char **bus_names;
  char **report_names = NULL;
  GList *all_report_list_entries = NULL;
  int i;
  int j;
  int n_names;
  GList *l;

  bus_names = get_all_session_bus_names ();

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
find_first_bus_with_report (const char *report_name)
{
  char **bus_names = get_all_session_bus_names ();
  char *bus_name = NULL;
  int i;

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

  return bus_name;
}

/* XXX: eventually this will be deprecated and we'll instead have a
 * UProfReportableProxy object instead. */
char *
uprof_dbus_get_text_report (const char *bus_name, const char *report_name)
{
  char *name;
  char *report_path;
  DBusGConnection *session_bus;
  DBusGProxy *proxy;
  GError *error;
  char *text_report;

  g_return_val_if_fail (report_name != NULL, FALSE);

  name = _uprof_dbus_canonify_name (g_strdup (report_name));
  if (!bus_name)
    bus_name = find_first_bus_with_report (name);

  if (!bus_name)
    return NULL;

  session_bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

  report_path = g_strdup_printf ("/org/freedesktop/UProf/Reports/%s",
                                 name);

  proxy = dbus_g_proxy_new_for_name (session_bus,
                                     bus_name,
                                     report_path,
                                     "org.freedesktop.UProf.Reportable");

  g_free (report_path);

  if (!dbus_g_proxy_call_with_timeout (proxy,
                                       "GetTextReport",
                                       1000,
                                       &error,
                                       G_TYPE_INVALID,
                                       G_TYPE_STRING, &text_report,
                                       G_TYPE_INVALID))
    {
      g_warning ("Failed to call GetTextReport: %s", error->message);
      g_error_free (error);
      text_report = NULL;
    }

#if 0
  dbus_g_proxy_call_no_reply (proxy,
                              "GetTextReport",
                              G_TYPE_INVALID,
                              G_TYPE_STRING, &text_report,
                              G_TYPE_INVALID);
#endif

  g_object_unref (proxy);

  return text_report;
}

void
uprof_dbus_reset_report (const char *bus_name, const char *report_name)
{
  char *name;
  char *report_path;
  DBusGConnection *session_bus;
  DBusGProxy *proxy;
  GError *error;

  g_return_if_fail (report_name != NULL);

  name = _uprof_dbus_canonify_name (g_strdup (report_name));
  if (!bus_name)
    bus_name = find_first_bus_with_report (name);

  if (!bus_name)
    return;

  session_bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

  report_path = g_strdup_printf ("/org/freedesktop/UProf/Reports/%s",
                                 name);

  proxy = dbus_g_proxy_new_for_name (session_bus,
                                     bus_name,
                                     report_path,
                                     "org.freedesktop.UProf.Reportable");

  g_free (report_path);

  if (!dbus_g_proxy_call_with_timeout (proxy,
                                       "Reset",
                                       1000,
                                       &error,
                                       G_TYPE_INVALID,
                                       G_TYPE_INVALID))
    {
      g_warning ("Failed to call Reset: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (proxy);
}

