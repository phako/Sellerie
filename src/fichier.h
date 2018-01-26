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

#ifndef FICHIER_H_
#define FICHIER_H_

#include <gtk/gtk.h>
#include <glib.h>

void send_raw_file(GtkWindow *parent);
void save_raw_file(GtkWindow *parent);
void add_input(void);

gboolean gt_file_get_waiting_for_char (void);
void gt_file_set_waiting_for_char (gboolean waiting);

const char *gt_file_get_default (void);
void gt_file_set_default (const char *file);


#endif
