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

#ifndef _UPROF_OBJECT_STATE_H_
#define _UPROF_OBJECT_STATE_H_

#include <glib.h>

G_BEGIN_DECLS

#ifndef UPROF_CONTEXT_TYPEDEF
typedef struct _UProfContext UProfContext;
#define UPROF_CONTEXT_TYPEDEF
#endif

typedef struct _UProfObjectState
{
  /*< private >*/
  UProfContext *context;

  char  *name;
  char  *description;
  GList *locations;

  unsigned long padding0;
  unsigned long padding1;
  unsigned long padding2;
  unsigned long padding3;
  unsigned long padding4;
  unsigned long padding5;
  unsigned long padding6;
  unsigned long padding7;
  unsigned long padding8;
  unsigned long padding9;

} UProfObjectState;

G_END_DECLS

#endif /* _UPROF_OBJECT_STATE_H_ */
