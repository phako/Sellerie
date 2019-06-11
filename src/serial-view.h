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

#include "buffer.h"

#include <glib-object.h>
#include <gtk/gtk.h>
#include <vte/vte.h>

enum _GtSerialViewMode { GT_SERIAL_VIEW_TEXT, GT_SERIAL_VIEW_HEX };

typedef enum _GtSerialViewMode GtSerialViewMode;

G_BEGIN_DECLS

GType gt_serial_view_get_type (void);
#define GT_TYPE_SERIAL_VIEW (gt_serial_view_get_type ())

G_DECLARE_FINAL_TYPE (GtSerialView, gt_serial_view, GT, SERIAL_VIEW, VteTerminal)

GtkWidget *
gt_serial_view_new (GtBuffer *buffer);
void gt_serial_view_clear (GtSerialView *self);
void gt_serial_view_set_show_index (GtSerialView *self,
                                    gboolean      show);
gboolean gt_serial_view_get_show_index (GtSerialView *self);

void gt_serial_view_set_bytes_per_line (GtSerialView *self,
                                        guint         bytes_per_line);

guint gt_serial_view_get_bytes_per_line (GtSerialView *self);

void gt_serial_view_inc_total_bytes (GtSerialView *self,
                                     guint         bytes);

guint gt_serial_view_get_total_bytes (GtSerialView *self);

void
gt_serial_view_set_display_mode (GtSerialView *self, GtSerialViewMode mode);

G_END_DECLS

#endif /* SERIAL_VIEW_H */
