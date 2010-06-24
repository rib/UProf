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
#include "uprof-service-glue.h"
#include "uprof-report.h"

#include <dbus/dbus-glib.h>
#include <glib/gprintf.h>
#include <string.h>

#define UPROF_SERVICE_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), UPROF_TYPE_SERVICE, UProfServicePrivate))

G_DEFINE_TYPE (UProfService, uprof_service, G_TYPE_OBJECT)

struct _UProfServicePrivate
{
  GList *reports;
};

static void
uprof_service_constructed (GObject *object)
{
  DBusGConnection *session_bus;
  GError *error = NULL;

  session_bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
  dbus_g_connection_register_g_object (session_bus,
                                       "/org/freedesktop/UProf/Service",
                                       object);
}

static void
uprof_service_class_init (UProfServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed  = uprof_service_constructed;

  g_type_class_add_private (klass, sizeof (UProfServicePrivate));
}

static void
uprof_service_init (UProfService *self)
{
  self->priv = UPROF_SERVICE_GET_PRIVATE (self);
}

void
_uprof_service_register_dbus_type_info (void)
{
  dbus_g_object_type_install_info (UPROF_TYPE_SERVICE,
                                   &dbus_glib__uprof_service_object_info);
}

UProfService *
_uprof_service_new (void)
{
  return g_object_new (UPROF_TYPE_SERVICE, NULL);
}

void
_uprof_service_add_report (UProfService *self, UProfReport *report)
{
  self->priv->reports = g_list_prepend (self->priv->reports, report);
}

void
_uprof_service_remove_report (UProfService *self, UProfReport *report)
{
  self->priv->reports = g_list_remove (self->priv->reports, report);
}

gboolean
_uprof_service_list_reports (UProfService *self,
                             char ***reports_ret,
                             GError *error)
{
  int len = g_list_length (self->priv->reports);
  char **strv = g_new (char *, len + 1);
  GList *l;
  int i;

  for (l = self->priv->reports, i = 0; l; l = l->next, i++)
    {
      UProfReport *report = l->data;
      strv[i] = g_strdup (uprof_report_get_name (report));
    }
  strv[i] = NULL;

  *reports_ret = strv;
  return TRUE;
}
