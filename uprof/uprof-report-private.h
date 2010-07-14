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

#ifndef _UPROF_REPORT_PRIVATE_H_
#define _UPROF_REPORT_PRIVATE_H_

void
_uprof_report_register_dbus_type_info (void);

gboolean
_uprof_report_get_text_report (UProfReport *report,
                               char **text_ret,
                               GError **error);
gboolean
_uprof_report_reset (UProfReport *report, GError **error);

gboolean
_uprof_report_enable_trace_messages (UProfReport *report,
                                     const char *context,
                                     GError **error);

gboolean
_uprof_report_disable_trace_messages (UProfReport *report,
                                      const char *context,
                                      GError **error);

#endif /* _UPROF_REPORT_PRIVATE_H_ */
