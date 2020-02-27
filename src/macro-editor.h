#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_MACRO_TYPE_EDITOR (gt_macro_editor_get_type ())

G_DECLARE_FINAL_TYPE (
    GtMacroEditor, gt_macro_editor, GT, MACRO_EDITOR, GtkDialog)

GtkWidget *
gt_macro_editor_new (void);

G_END_DECLS
