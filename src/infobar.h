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

#ifndef GT_INFOBAR_H
#define GT_INFOBAR_H

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_INFOBAR (gt_infobar_get_type())

G_DECLARE_FINAL_TYPE (GtInfobar, gt_infobar, GT, INFOBAR, GtkInfoBar)

GtkWidget *gt_infobar_new (void);
void gt_infobar_set_label (GtInfobar *, const char *);
void gt_infobar_set_progress (GtInfobar *, double);

G_END_DECLS

#endif /* GT_INFOBAR_H */

