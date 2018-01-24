/*
 *   This file is part of GtkTerm.
 *
 *   GtkTerm is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   GtkTerm is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with GtkTerm.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GT_LOGGING_H
#define GT_LOGGING_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GT_TYPE_LOGGING (gt_logging_get_type())

G_DECLARE_FINAL_TYPE (GtLogging, gt_logging, GT, LOGGING, GObject)

GtLogging *gt_logging_new (void);
gboolean gt_logging_start(GtLogging *logger, const char *file_name);
void gt_logging_pause_resume(GtLogging *logger);
void gt_logging_stop(GtLogging *logger);
void gt_logging_clear(GtLogging *logger);
void gt_logging_log(GtLogging *logger, const char *chars, size_t size);
gboolean gt_logging_get_active(GtLogging *logger);
const char *gt_logging_get_default_file(GtLogging *logger);
G_END_DECLS

#endif /* GT_LOGGING_H */

