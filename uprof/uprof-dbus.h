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

#ifndef _UPROF_DBUS_H_
#define _UPROF_DBUS_H_

#include <glib.h>

#include <uprof-report-proxy.h>

G_BEGIN_DECLS

/**
 * UPROF_DBUS_ERROR:
 *
 * #GError domain for the uprof dbus API
 *
 * Since: 0.4
 */
#define UPROF_DBUS_ERROR (uprof_dbus_error_quark ())

/**
 * UProfDbusError:
 * @UPROF_DBUS_ERROR_UNKNOWN_REPORT: Given report name could not be found
 *
 * Error enumeration for the uprof dbus API.
 *
 * Since: 0.4
 */
typedef enum { /*< prefix=UPROF_DBUS_ERROR >*/
  UPROF_DBUS_ERROR_UNKNOWN_REPORT,
  UPROF_DBUS_ERROR_DISCONNECTED
} UProfDBusError;

GQuark
uprof_dbus_error_quark (void);

char **
uprof_dbus_list_reports (GError **error);

UProfReportProxy *
uprof_dbus_get_report_proxy (const char *report_location,
                             GError **error);

G_END_DECLS

#endif /* _UPROF_DBUS_H_ */

