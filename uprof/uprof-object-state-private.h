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

#ifndef _UPROF_OBJECT_STATE_PRIVATE_H_
#define _UPROF_OBJECT_STATE_PRIVATE_H_

#define UPROF_OBJECT_STATE(X) ((UProfObjectState *)(X))

typedef struct _UProfObjectLocation
{
  char  *filename;
  long   line;
  char  *function;
} UProfObjectLocation;

void
_uprof_object_state_init (UProfObjectState *object,
                          UProfContext *context,
                          const char *name,
                          const char *description);

void
_uprof_object_state_dispose (UProfObjectState *object);

void
_uprof_object_state_add_location (UProfObjectState *object,
                                  const char       *filename,
                                  unsigned long     line,
                                  const char       *function);

#endif /* _UPROF_OBJECT_STATE_PRIVATE_H_ */

