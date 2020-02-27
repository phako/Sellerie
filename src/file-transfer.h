/*
 *   This file is part of Sellerie.
 *
 *   Sellerie is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Sellerie is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Sellerie.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GT_TYPE_FILE_TRANSFER (gt_file_transfer_get_type ())

G_DECLARE_FINAL_TYPE (
    GtFileTransfer, gt_file_transfer, GT, FILE_TRANSFER, GObject)

void
gt_file_transfer_start (GtFileTransfer *self,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer user_data);
gboolean
gt_file_transfer_finish (GtFileTransfer *self,
                         GAsyncResult *res,
                         GError **error);

G_END_DECLS
