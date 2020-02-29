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

#ifndef MACROS_H_
#define MACROS_H_

#include <gtk/gtk.h>

#include <glib.h>

typedef struct _GtMacro GtMacro;

void Config_macros(GtkWindow *parent);
void remove_shortcuts(void);
void add_shortcuts(void);
void
create_shortcuts (GList *);
GList *
get_shortcuts (void);
char *
serialize_macro (GtMacro *macro);
GtMacro *
gt_macro_from_string (const char *str);

GtMacro *
gt_macro_new (const char *shortcut, const char *action);

#endif
