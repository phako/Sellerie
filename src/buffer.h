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

#ifndef BUFFER_H
#define BUFFER_H

#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define GT_TYPE_BUFFER gt_buffer_get_type ()
G_DECLARE_FINAL_TYPE (GtBuffer, gt_buffer, GT, BUFFER, GObject)

typedef struct _GtBuffer GtBuffer;

typedef void (*GtBufferFunc) (char *, unsigned int, gpointer);

GtBuffer *gt_buffer_new (void);

void
gt_buffer_put_bytes (GtBuffer *, GBytes *, gboolean);
void
gt_buffer_put_chars (GtBuffer *, const char *, unsigned int, gboolean);
void gt_buffer_clear (GtBuffer *);
void gt_buffer_write (GtBuffer *);
gboolean gt_buffer_write_to_file (GtBuffer *, const char *, GError **);

G_END_DECLS

#endif
