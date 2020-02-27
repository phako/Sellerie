#include "macro-editor.h"

#include <glib/gi18n.h>

enum { COLUMN_SHORTCUT, COLUMN_ACTION, NUM_COLUMNS };

struct _GtMacroEditor {
    GtkDialog parent_instance;

    GtkCellRenderer *cellrenderer_shortcut;
    GtkTreeViewColumn *column_shortcut;
};

G_DEFINE_TYPE (GtMacroEditor, gt_macro_editor, GTK_TYPE_DIALOG)

enum { PROP_0, N_PROPS };

static GParamSpec *properties[N_PROPS];

static void
add_shortcut (GtkButton *button, gpointer pointer)
{
    GtkTreeIter iter;
    GtkTreeModel *model = (GtkTreeModel *)pointer;

    gtk_list_store_append (GTK_LIST_STORE (model), &iter);

    gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, NULL, 1, NULL, -1);
}

static void
remove_shortcut (GtkButton *button, gpointer pointer)
{
    GtkTreeIter iter;
    GtkTreeView *treeview = GTK_TREE_VIEW (pointer);
    GtkTreeModel *model = gtk_tree_view_get_model (treeview);
    GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);

    if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
    }
}

static void
accel_set_func (GtkTreeViewColumn *tree_column,
                GtkCellRenderer *cell,
                GtkTreeModel *model,
                GtkTreeIter *iter,
                gpointer data)
{
    char *keycode = NULL;
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

static void
show_help (GtkButton *button, gpointer pointer)
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

GtkWidget *
gt_macro_editor_new (void)
{
    return g_object_new (GT_MACRO_TYPE_EDITOR, NULL);
}

static void
gt_macro_editor_finalize (GObject *object)
{
    GtMacroEditor *self = (GtMacroEditor *)object;

    G_OBJECT_CLASS (gt_macro_editor_parent_class)->finalize (object);
}

static void
gt_macro_editor_get_property (GObject *object,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    GtMacroEditor *self = GT_MACRO_EDITOR (object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gt_macro_editor_set_property (GObject *object,
                              guint prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    GtMacroEditor *self = GT_MACRO_EDITOR (object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gt_macro_editor_class_init (GtMacroEditorClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gt_macro_editor_finalize;
    object_class->get_property = gt_macro_editor_get_property;
    object_class->set_property = gt_macro_editor_set_property;

    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gtk_widget_class_set_template_from_resource (
        widget_class, "/org/jensge/Sellerie/macros.ui");
    gtk_widget_class_bind_template_child (
        widget_class, GtMacroEditor, cellrenderer_shortcut);
    gtk_widget_class_bind_template_child (
        widget_class, GtMacroEditor, column_shortcut);
    gtk_widget_class_bind_template_callback (widget_class, "gt_macros_save");
    gtk_widget_class_bind_template_callback_full (
        widget_class, "shortcut_edited", G_CALLBACK (shortcut_edited));
    gtk_widget_class_bind_template_callback_full (
        widget_class, "gt_macros_add_shortcut", G_CALLBACK (add_shortcut));
    gtk_widget_class_bind_template_callback_full (widget_class,
                                                  "gt_macros_remove_shortcut",
                                                  G_CALLBACK (remove_shortcut));

    gtk_widget_class_bind_template_callback_full (
        widget_class, "accel_edited", G_CALLBACK (accel_edited_callback));
    gtk_widget_class_bind_template_callback_full (
        widget_class, "accel_cleared", G_CALLBACK (accel_cleared_callback));
    gtk_widget_class_bind_template_callback_full (
        widget_class, "show_help", G_CALLBACK (show_help));
}

static void
gt_macro_editor_init (GtMacroEditor *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
    gtk_tree_view_column_set_cell_data_func (self->column_shortcut,
                                             self->cellrenderer_shortcut,
                                             accel_set_func,
                                             NULL,
                                             NULL);
}
