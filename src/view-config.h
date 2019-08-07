/* view-config.h
 *
 * Copyright (C) 2019 Jens Georg <mail@jensge.org>
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

#pragma once

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "serial-view.h"

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

GType
gt_view_config_get_type (void);
#define GT_TYPE_VIEW_CONFIG (gt_view_config_get_type ())

G_DECLARE_FINAL_TYPE (GtViewConfig, gt_view_config, GT, VIEW_CONFIG, GtkDialog)

GtkWidget *
gt_view_config_new (GtSerialView *view);

G_END_DECLS
