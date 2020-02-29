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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "macros.h"
#include "macro-manager.h"
#include "main-window.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

extern GtkWidget *Fenetre;

void
gt_macros_save (GtkButton *button, gpointer pointer);

enum { COLUMN_SHORTCUT, COLUMN_ACTION, COLUMN_ID, NUM_COLUMNS };

static GtkWidget *window = NULL;


static gboolean
build_macro_list (GtkTreeModel *model,
                  GtkTreePath *path,
                  GtkTreeIter *iter,
                  gpointer data)
{
    GtMacroManager *manager = GT_MACRO_MANAGER (data);
    g_autofree char *shortcut;
    g_autofree char *action;

    gtk_tree_model_get (model,
                        iter,
                        COLUMN_SHORTCUT,
                        &(shortcut),
                        COLUMN_ACTION,
                        &(action),
                        -1);

    gt_macro_manager_add (manager, shortcut, action, "");

    return FALSE;
}

void
gt_macros_save (GtkButton *button, gpointer pointer)
{
    GtMacroManager *manager = gt_macro_manager_get_default ();

    gt_macro_manager_clear (manager);

    gtk_tree_model_foreach (gtk_tree_view_get_model (GTK_TREE_VIEW (pointer)),
                            build_macro_list,
                            manager);
}

