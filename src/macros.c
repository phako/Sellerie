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
#include "main-window.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

struct _GtMacro {
    gchar *shortcut;
    gchar *action;
    GClosure *closure;
};

extern GtkWidget *Fenetre;

void
gt_macros_add_shortcut (GtkButton *button, gpointer pointer);
void
gt_macros_remove_shortcut (GtkButton *button, gpointer pointer);
void
gt_macros_save (GtkButton *button, gpointer pointer);
void
gt_macros_show_help (GtkButton *button, gpointer pointer);

enum { COLUMN_SHORTCUT, COLUMN_ACTION, NUM_COLUMNS };

static GList *macros = NULL;
static GtkWidget *window = NULL;

GList *
get_shortcuts (void)
{
    return macros;
}

static void
shortcut_callback (gpointer number)
{
    gchar *string;
    gchar *str;
    gint i, length;
    guchar a;
    guint val_read;
    GtMacro *macro =
        (GtMacro *)g_list_nth_data (macros, GPOINTER_TO_INT (number));

    if (macro == NULL)
        return;

    string = macro->action;
    length = strlen (string);

    for (i = 0; i < length; i++) {
        if (string[i] == '\\') {
            if (g_unichar_isdigit ((gunichar)string[i + 1])) {
                if ((string[i + 1] == '0') && (string[i + 2] != 0)) {
                    if (g_unichar_isxdigit ((gunichar)string[i + 3])) {
                        str = &string[i + 2];
                        i += 3;
                    } else {
                        str = &string[i + 1];
                        if (g_unichar_isxdigit ((gunichar)string[i + 2]))
                            i += 2;
                        else
                            i++;
                    }
                } else {
                    str = &string[i + 1];
                    if (g_unichar_isxdigit ((gunichar)string[i + 2]))
                        i += 2;
                    else
                        i++;
                }
                if (sscanf (str, "%02X", &val_read) == 1)
                    a = (guchar)val_read;
                else
                    a = '\\';
            } else {
                switch (string[i + 1]) {
                case 'a':
                    a = '\a';
                    break;
                case 'b':
                    a = '\b';
                    break;
                case 't':
                    a = '\t';
                    break;
                case 'n':
                    a = '\n';
                    break;
                case 'v':
                    a = '\v';
                    break;
                case 'f':
                    a = '\f';
                    break;
                case 'r':
                    a = '\r';
                    break;
                case '\\':
                    a = '\\';
                    break;
                default:
                    a = '\\';
                    i--;
                    break;
                }
                i++;
            }
            gt_serial_port_send_chars (
                GT_MAIN_WINDOW (Fenetre)->serial_port, (gchar *)&a, 1);
        } else {
            gt_serial_port_send_chars (
                GT_MAIN_WINDOW (Fenetre)->serial_port, &string[i], 1);
        }
    }

    str = g_strdup_printf (_ ("Macro \"%s\" sent !"), macro->shortcut);
    gt_main_window_temp_message (GT_MAIN_WINDOW (Fenetre), str, 800);
    g_free (str);
}

void
create_shortcuts (GList *macro)
{
    macros = macro;
}

void
add_shortcuts (void)
{
    long i = 0;
    guint acc_key;
    GdkModifierType mod;
    GList *it = macros;

    for (it = macros; it != NULL; it = it->next) {
        GtMacro *macro = (GtMacro *)it->data;

        macro->closure = g_cclosure_new_swap (
            G_CALLBACK (shortcut_callback), GINT_TO_POINTER (i), NULL);
        gtk_accelerator_parse (macro->shortcut, &acc_key, &mod);
        if (acc_key != 0)
            gt_main_window_add_shortcut (
                GT_MAIN_WINDOW (Fenetre), acc_key, mod, macro->closure);
        i++;
    }
}

char *
serialize_macro (GtMacro *macro)
{
    return g_strdup_printf ("%s::%s", macro->shortcut, macro->action);
}

GtMacro *
gt_macro_new (const char *shortcut, const char *action)
{
    g_return_val_if_fail (shortcut != NULL, NULL);
    g_return_val_if_fail (action != NULL, NULL);

    GtMacro *macro = g_new0 (GtMacro, 1);
    macro->shortcut = g_strdup (shortcut);
    macro->action = g_strdup (action);

    return macro;
}

GtMacro *
gt_macro_from_string (const char *str)
{
    g_return_val_if_fail (str != NULL, NULL);

    gchar **parts = g_strsplit (str, "::", 2);
    if (g_strv_length (parts) != 2) {
        g_warning ("Failed to parse macro \"%s\"", str);

        return NULL;
    }

    GtMacro *macro = g_new0 (GtMacro, 1);
    macro->shortcut = parts[0];
    macro->action = parts[1];

    /* Only free the array, the macro owns the parts now */
    g_free (parts);

    return macro;
}

static void
free_macro (gpointer data)
{
    GtMacro *macro = (GtMacro *)data;

    g_free (macro->shortcut);
    g_free (macro->action);
    // g_closure_unref (macro->closure);
    g_free (macro);
}

static void
remove_macro (gpointer data, gpointer user_data)
{
    GtMacro *macro = (GtMacro *)data;
    if (macro->shortcut != NULL) {
        gt_main_window_remove_shortcut (GT_MAIN_WINDOW (user_data),
                                        macro->closure);
    }
}

static void
macros_destroy (void)
{
    g_list_free_full (macros, free_macro);
    macros = NULL;
}

void
remove_shortcuts (void)
{
    g_list_foreach (macros, remove_macro, Fenetre);
    macros_destroy ();
}

static void
fill_model (gpointer data, gpointer user_data)
{
    GtMacro *macro = (GtMacro *)data;
    GtkTreeIter iter;

    gtk_list_store_append (GTK_LIST_STORE (user_data), &iter);
    gtk_list_store_set (GTK_LIST_STORE (user_data),
                        &iter,
                        COLUMN_SHORTCUT,
                        macro->shortcut,
                        COLUMN_ACTION,
                        macro->action,
                        -1);
}

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
    GList **macros_list = (GList **)data;
    GtMacro *macro = g_new0 (GtMacro, 1);
    gtk_tree_model_get (model,
                        iter,
                        COLUMN_SHORTCUT,
                        &(macro->shortcut),
                        COLUMN_ACTION,
                        &(macro->action),
                        -1);
    *macros_list = g_list_prepend (*macros_list, macro);

    return TRUE;
}

void
gt_macros_save (GtkButton *button, gpointer pointer)
{
    remove_shortcuts ();

    gtk_tree_model_foreach (gtk_tree_view_get_model (GTK_TREE_VIEW (pointer)),
                            build_macro_list,
                            &macros);
    macros = g_list_reverse (macros);

    add_shortcuts ();
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
    g_list_foreach (macros, fill_model, GTK_LIST_STORE (model));
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
