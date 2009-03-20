/*
 * This file is part of UProf.
 *
 * Copyright © 2008, 2009 Robert Bragg
 *
 * UProf is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * UProf is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with UProf.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _UPROF_PRIVATE_H_
#define _UPROF_PRIVATE_H_

#include <glib.h>

G_BEGIN_DECLS

typedef struct _UProfObjectState
{
  char  *name;
  GList *locations;
} UProfObjectState;

typedef struct _UProfCounterState
{
  UProfObjectState  object;

  unsigned long     count;

} UProfCounterState;

typedef struct _UProfTimerState UProfTimerState;
struct _UProfTimerState
{
  UProfObjectState    object;

  char               *parent_name;

  unsigned long       count;
  unsigned long long  start;
  unsigned long long  total;

  unsigned long long  fastest;
  unsigned long long  slowest;

  /* note: not resolved until sorting @ report time */
  UProfTimerState    *parent;
  GList              *children;

};

G_END_DECLS

#endif /* _UPROF_PRIVATE_H_ */
