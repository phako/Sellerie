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
shortcut_edited (GtkCellRendererText *cell,
                 const gchar *path_string,
                 const gchar *new_text,
                 gpointer data)
{
    GtkTreeModel *model = (GtkTreeModel *)data;
    GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
    GtkTreeIter iter;

    gtk_tree_model_get_iter (model, &iter, path);

    gtk_list_store_set (
        GTK_LIST_STORE (model), &iter, COLUMN_ACTION, new_text, -1);
    gtk_tree_path_free (path);

    return TRUE;
}

void
gt_macros_add_shortcut (GtkButton *button, gpointer pointer)
{
    GtkTreeIter iter;
    GtkTreeModel *model = (GtkTreeModel *)pointer;

    gtk_list_store_append (GTK_LIST_STORE (model), &iter);

    gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, NULL, 1, NULL, -1);
}

void
gt_macros_remove_shortcut (GtkButton *button, gpointer pointer)
{
    GtkTreeIter iter;
    GtkTreeView *treeview = GTK_TREE_VIEW (pointer);
    GtkTreeModel *model = gtk_tree_view_get_model (treeview);
    GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);

    if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
    }
}

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
gt_macros_show_help (GtkButton *button, gpointer pointer)
{
    GtkWidget *Dialog;

    Dialog = gtk_message_dialog_new (
        GTK_WINDOW (pointer),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_CLOSE,
        _ ("The \"action\" field of a macro is the data to be sent on the "
           "port. Text can be entered, but also special chars, like \\n, \\t, "
           "\\r, etc. You can also enter hexadecimal data preceded by a '\\'. "
           "The hexadecimal data should not begin with a letter (eg. use \\0FF "
           "and not \\FF)\nExamples :\n\t\"Hello\\n\" sends \"Hello\" followed "
           "by a Line Feed\n\t\"Hello\\0A\" does the same thing but the LF is "
           "entered in hexadecimal"));

    gtk_dialog_run (GTK_DIALOG (Dialog));
    gtk_widget_destroy (Dialog);
}

static void
accel_set_func (GtkTreeViewColumn *tree_column,
                GtkCellRenderer *cell,
                GtkTreeModel *model,
                GtkTreeIter *iter,
                gpointer data)
{
    char *keycode;
    guint keyval = 0;
    GdkModifierType mask = 0;

    gtk_tree_model_get (model, iter, 0, &keycode, -1);
    if (keycode != NULL) {
        gtk_accelerator_parse (keycode, &keyval, &mask);
    }

    g_object_set (cell,
                  "visible",
                  TRUE,
                  "sensitive",
                  TRUE,
                  "editable",
                  TRUE,
                  "accel-key",
                  keyval,
                  "accel-mods",
                  mask,
                  NULL);
    g_free (keycode);
}

static void
accel_edited_callback (GtkCellRendererAccel *cell,
                       gchar *path_string,
                       guint keyval,
                       GdkModifierType mask,
                       guint hardware_keycode,
                       GtkTreeView *view)
{
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter;
    char *name = NULL;

    model = gtk_tree_view_get_model (view);

    path = gtk_tree_path_new_from_string (path_string);
    if (!path)
        return;

    if (!gtk_tree_model_get_iter (model, &iter, path)) {
        gtk_tree_path_free (path);

        return;
    }
    gtk_tree_path_free (path);

    name = gtk_accelerator_name (keyval, mask);

    gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, name, -1);
    g_free (name);
}

static void
accel_cleared_callback (GtkCellRendererAccel *cell,
                        gchar *path_string,
                        GtkTreeView *view)
{
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter;

    model = gtk_tree_view_get_model (view);

    path = gtk_tree_path_new_from_string (path_string);
    if (!path)
        return;

    if (!gtk_tree_model_get_iter (model, &iter, path)) {
        gtk_tree_path_free (path);
        return;
    }
    gtk_tree_path_free (path);

    gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, NULL, -1);
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
    g_signal_connect (renderer, "edited", G_CALLBACK (shortcut_edited), model);

    column = GTK_TREE_VIEW_COLUMN (
        gtk_builder_get_object (builder, "column_shortcut"));
    renderer = GTK_CELL_RENDERER (
        gtk_builder_get_object (builder, "cellrenderer_shortcut"));
    gtk_tree_view_column_set_cell_data_func (
        column, renderer, accel_set_func, NULL, NULL);

    g_signal_connect (
        renderer, "accel-edited", G_CALLBACK (accel_edited_callback), treeview);
    g_signal_connect (renderer,
                      "accel-cleared",
                      G_CALLBACK (accel_cleared_callback),
                      treeview);

    gtk_widget_show (GTK_WIDGET (window));
    g_object_unref (builder);
}
