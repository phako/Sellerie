/* serial-view.h
 *
 * Copyright (C) 2016 Jens Georg <mail@jensge.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SERIAL_VIEW_H
#define SERIAL_VIEW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <gtk/gtk.h>
#include <vte/vte.h>

G_BEGIN_DECLS

GType gt_serial_view_get_type (void);
#define GT_TYPE_SERIAL_VIEW (gt_serial_view_get_type ())
#define GT_SERIAL_VIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST (obj), GT_TYPE_SERIAL_VIEW, GtSerialView)
#define GT_SERIAL_VIEW_CLASS(klass) (GTYPE_CHECK_CLASS_CAST (klass), GT_TYPE_SERIAL_VIEW, GtSerialViewClass)
#define GT_IS_SERIAL_VIEW(obj) (G_TYPE_CHECK_INSTANCE_TYPE (obj), GT_TYPE_SERIAL_VIEW)
#define GT_IS_SERIAL_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE (klass), GT_TYPE_SERIAL_VIEW)

typedef struct _GtSerialView GtSerialView;
typedef struct _GtSerialViewClass GtSerialViewClass;

GtkWidget *gt_serial_view_new (void);

G_END_DECLS

#endif /* SERIAL_VIEW_H */
