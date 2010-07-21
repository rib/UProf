/* This file is part of UProf.
 *
 * Copyright © 2008, 2009, 2010 Robert Bragg
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

#ifndef _UPROF_CONTEXT_PRIVATE_H_
#define _UPROF_CONTEXT_PRIVATE_H_

#include <glib.h>

struct _UProfContext
{
  /*< private >*/

  guint  ref;

  char	*name;

  GList *links;

  GList *statistics_groups;

  GList	*counters;
  GList	*timers;

  int disabled;

  gboolean resolved;
  GList *root_timers;

  GList *report_messages;

  int tracing_enabled;
  GList *trace_message_callbacks;
  int next_trace_message_callbacks_id;

  GList *options;
};

typedef void (*UProfContextCallback) (UProfContext *context,
                                      gpointer user_data);

void
_uprof_context_for_self_and_links_recursive (UProfContext *context,
                                             UProfContextCallback callback,
                                             gpointer user_data);

void
_uprof_context_reset (UProfContext *context);

typedef void (*UProfContextTraceMessageCallback) (UProfContext *context,
                                                  const char *message,
                                                  void *user_data);

int
_uprof_context_add_trace_message_callback (
                                     UProfContext *context,
                                     UProfContextTraceMessageCallback callback,
                                     void *user_data);

void
_uprof_context_remove_trace_message_callback (UProfContext *context,
                                              int id);

gboolean
_uprof_context_get_boolean_option (UProfContext *context,
                                   const char *name,
                                   gboolean *value,
                                   GError **error);

gboolean
_uprof_context_set_boolean_option (UProfContext *context,
                                   const char *name,
                                   gboolean value,
                                   GError **error);

void
_uprof_context_append_options_xml (UProfContext *context,
                                   GString *options_xml);

#endif /* _UPROF_CONTEXT_PRIVATE_H_ */

