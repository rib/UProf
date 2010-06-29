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

#include "uprof-object-state.h"
#include "uprof-object-state-private.h"

#include <glib.h>

#include <string.h>

void
_uprof_object_state_init (UProfObjectState *object,
                          UProfContext *context,
                          const char *name,
                          const char *description)
{
  object->context = context;
  object->name = g_strdup (name);
  object->description = g_strdup (description);
  object->locations = NULL;
}

void
free_locations (GList *locations)
{
  GList *l;
  for (l = locations; l != NULL; l = l->next)
    {
      UProfObjectLocation *location = l->data;
      g_free (location->filename);
      g_free (location->function);
      g_slice_free (UProfObjectLocation, l->data);
    }
  g_list_free (locations);
}

void
_uprof_object_state_dispose (UProfObjectState *object)
{
  g_free (object->name);
  g_free (object->description);
  free_locations (object->locations);
}

/* A counter or timer may be accessed from multiple places in source
 * code so we support tracking a list of locations for each uprof
 * object. Note: we don't currently separate statistics for each
 * location, though that might be worth doing. */
void
_uprof_object_state_add_location (UProfObjectState *object,
                                  const char       *filename,
                                  unsigned long     line,
                                  const char       *function)

{
  GList *l;
  UProfObjectLocation *location;

  for (l = object->locations; l != NULL; l = l->next)
    {
      location = l->data;
      if (strcmp (location->filename, filename) == 0
          && location->line == line
          && strcmp (location->function, function) == 0)
        return;
    }

  location = g_slice_alloc (sizeof (UProfObjectLocation));
  location->filename = g_strdup (filename);
  location->line = line;
  location->function = g_strdup (function);
  object->locations = g_list_prepend (object->locations, location);
}

