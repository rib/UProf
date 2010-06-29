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
};

typedef void (*UProfContextCallback) (UProfContext *context,
                                      gpointer user_data);

void
_uprof_context_for_self_and_links_recursive (UProfContext *context,
                                             UProfContextCallback callback,
                                             gpointer user_data);

#endif /* _UPROF_CONTEXT_PRIVATE_H_ */

