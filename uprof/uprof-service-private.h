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

#ifndef _UPROF_SERVICE_PRIVATE_H_
#define _UPROF_SERVICE_PRIVATE_H_

#include <glib.h>
#include <glib-object.h>

#define UPROF_TYPE_SERVICE              (uprof_service_get_type ())
#define UPROF_SERVICE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), UPROF_TYPE_SERVICE, UProfService))
#define UPROF_SERVICE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), UPROF_TYPE_SERVICE, UProfServiceClass))
#define UPROF_IS_SERVICE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), UPROF_TYPE_SERVICE))
#define UPROF_IS_SERVICE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), UPROF_TYPE_SERVICE))
#define UPROF_SERVICE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), UPROF_TYPE_SERVICE, UProfServiceClass))

typedef struct _UProfService UProfService;
typedef struct _UProfServiceClass UProfServiceClass;
typedef struct _UProfServicePrivate UProfServicePrivate;

GType uprof_service_get_type (void);

struct _UProfService
{
  GObject parent;
  UProfServicePrivate *priv;
};

struct _UProfServiceClass
{
  GObjectClass parent;
};

UProfService *
_uprof_service_new ();

void
_uprof_service_register_dbus_type_info (void);

#ifndef UPROF_REPORT_TYPEDEF
typedef struct _UProfReport UProfReport;
#define UPROF_REPORT_TYPEDEF
#endif

void
_uprof_service_add_report (UProfService *self, UProfReport *report);

void
_uprof_service_remove_report (UProfService *self, UProfReport *report);

gboolean
_uprof_service_list_reports (UProfService *self,
                             char ***reports_ret,
                             GError *error);

#endif /* _UPROF_SERVICE_PRIVATE_H_ */
