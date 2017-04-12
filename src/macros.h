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

#ifndef MACROS_H_
#define MACROS_H_

#include <gtk/gtk.h>

typedef struct
{
  gchar *shortcut;
  gchar *action;
  GClosure *closure;
}
macro_t;

void Config_macros(GtkWindow *parent);
void remove_shortcuts(void);
void add_shortcuts(void);
void create_shortcuts(macro_t *, gint);
macro_t *get_shortcuts(gint *);

#endif
