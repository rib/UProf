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

#ifndef _UPROF_UPROF_REPORT_H_
#define _UPROF_UPROF_REPORT_H_

#include <glib.h>

G_BEGIN_DECLS

/**
 * UProfReportProxy:
 *
 * Provides a convenient proxy object based API to access a UProf reportable
 * service over DBus.
 */
typedef struct _UProfReportProxy UProfReportProxy;

UProfReportProxy *
uprof_report_proxy_ref (UProfReportProxy *proxy);

void
uprof_report_proxy_unref (UProfReportProxy *proxy);

char *
uprof_report_proxy_get_text_report (UProfReportProxy *proxy,
                                    GError **error);

gboolean
uprof_report_proxy_reset (UProfReportProxy *proxy,
                          GError **error);

typedef void (*UProfReportProxyTraceMessageFilter) (UProfReportProxy *proxy,
                                                    const char *context,
                                                    const char *categories,
                                                    const char *location,
                                                    const char *message,
                                                    void *user_data);
int
uprof_report_proxy_add_trace_message_filter (
                                     UProfReportProxy *proxy,
                                     const char *context,
                                     UProfReportProxyTraceMessageFilter filter,
                                     void *user_data,
                                     GError **error);

gboolean
uprof_report_proxy_remove_trace_message_filter (UProfReportProxy *proxy,
                                                int id,
                                                GError **error);

typedef enum
{
  UPROF_REPORT_PROXY_OPTION_TYPE_BOOLEAN,
} UProfReportProxyOptionType;

typedef struct
{
  UProfReportProxyOptionType type;
  char *context;
  char *group;
  char *name;
  char *name_formatted;
  char *description;
} UProfReportProxyOption;

typedef gboolean (*UProfReportProxyOptionCallback) (
                                               UProfReportProxy *proxy,
                                               const char *context,
                                               UProfReportProxyOption *option,
                                               void *user_data);

gboolean
uprof_report_proxy_foreach_option (UProfReportProxy *proxy,
                                   const char *context,
                                   UProfReportProxyOptionCallback callback,
                                   void *user_data,
                                   GError **error);

gboolean
uprof_report_proxy_get_boolean_option (UProfReportProxy *proxy,
                                       const char *context,
                                       const char *name,
                                       gboolean *value,
                                       GError **error);

gboolean
uprof_report_proxy_set_boolean_option (UProfReportProxy *proxy,
                                       const char *context,
                                       const char *name,
                                       gboolean value,
                                       GError **error);

G_END_DECLS

#endif /* _UPROF_UPROF_REPORT_H_ */

