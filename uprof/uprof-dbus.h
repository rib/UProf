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
#include <glib-object.h>

G_BEGIN_DECLS

char **
uprof_dbus_list_reports (void);

#if 0
UProfDBusReportProxy *
uprof_dbus_get_report_proxy (char *report_location);
#endif

char *
uprof_dbus_get_text_report (const char *bus_name, const char *report_name);

G_END_DECLS

#endif /* _UPROF_DBUS_H_ */

