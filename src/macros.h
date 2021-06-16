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

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE (
    GtMacroManager, gt_macro_manager, GT, MACRO_MANAGER, GObject)

GtMacroManager *
gt_macro_manager_new (GtkApplication *app);

void
gt_macro_manager_add_from_string (GtMacroManager *self, const char *str);


const char *
gt_macro_manager_get_bytes (GtMacroManager *self,
                            const char *id,
                            gsize *length);

typedef struct _GtMacro GtMacro;

void
Config_macros (GtkWindow *parent, GtMacroManager *macro_manager);

GList *
gt_macro_manager_get_macros (GtMacroManager *self);

const char*
gt_macro_manager_get_shortcut (GtMacroManager *self, const char *id);

#if 0
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

G_END_DECLS

#endif
