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
gt_macros_add_shortcut (GtkButton *button, gpointer pointer);
void
gt_macros_remove_shortcut (GtkButton *button, gpointer pointer);
void
gt_macros_save (GtkButton *button, gpointer pointer);
void
gt_macros_show_help (GtkButton *button, gpointer pointer);

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


void
Config_macros (GtkWindow *parent)
{
    GtkBuilder *builder = NULL;
    GtkWidget *treeview = NULL;
    GtkTreeModel *model = NULL;
    GtkCellRenderer *renderer = NULL;
    GtkTreeViewColumn *column = NULL;

    builder = gtk_builder_new_from_resource ("/org/jensge/Sellerie/macros.ui");
    gtk_builder_connect_signals (builder, NULL);
    window = GTK_WIDGET (gtk_builder_get_object (builder, "dialog-macros"));
    gtk_window_set_transient_for (GTK_WINDOW (window), parent);
    treeview = GTK_WIDGET (gtk_builder_get_object (builder, "treeview"));
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
    GtMacroManager *manager = gt_macro_manager_get_default ();
    GListModel *list_model = gt_macro_manager_get_model (manager);
    for (guint i = 0; i < g_list_model_get_n_items (list_model); i++) {
        g_autoptr (GtMacro) macro = GT_MACRO (g_list_model_get_object (list_model, i));
        gtk_list_store_insert_with_values (GTK_LIST_STORE (model),
                                           NULL,
                                           -1,
                                           COLUMN_SHORTCUT,
                                           gt_macro_get_shortcut (macro),
                                           COLUMN_ACTION,
                                           gt_macro_get_action (macro),
                                           -1);
    }
    renderer = GTK_CELL_RENDERER (
        gtk_builder_get_object (builder, "cellrenderer_action"));
    // g_signal_connect (renderer, "edited", G_CALLBACK (shortcut_edited),
    // model);

    column = GTK_TREE_VIEW_COLUMN (
        gtk_builder_get_object (builder, "column_shortcut"));
    renderer = GTK_CELL_RENDERER (
        gtk_builder_get_object (builder, "cellrenderer_shortcut"));
#if 0
    gtk_tree_view_column_set_cell_data_func (
        column, renderer, accel_set_func, NULL, NULL);
    g_signal_connect (
        renderer, "accel-edited", G_CALLBACK (accel_edited_callback), treeview);
    g_signal_connect (renderer,
                      "accel-cleared",
                      G_CALLBACK (accel_cleared_callback),
                      treeview);
#endif

    gtk_widget_show (GTK_WIDGET (window));
    g_object_unref (builder);
}
