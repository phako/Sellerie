// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GT_TYPE_MACRO_MANAGER (gt_macro_manager_get_type ())

G_DECLARE_FINAL_TYPE (
    GtMacroManager, gt_macro_manager, GT, MACRO_MANAGER, GObject)

#define GT_TYPE_MACRO (gt_macro_get_type ())

G_DECLARE_FINAL_TYPE (GtMacro, gt_macro, GT, MACRO, GObject)

char *
gt_macro_to_string (GtMacro *self);

const char *
gt_macro_get_shortcut (GtMacro *self);

const char *
gt_macro_get_action (GtMacro *self);

GBytes *
gt_macro_get_bytes (GtMacro *self);

GtMacroManager *
gt_macro_manager_get_default ();

GtMacroManager *
gt_macro_manager_new (GApplication *app);

GListModel *
gt_macro_manager_get_model (GtMacroManager *manager);

const char *
gt_macro_manager_add_from_string (GtMacroManager *self,
                                  const char *macro_string);

const char *
gt_macro_manager_add (GtMacroManager *self,
                      const char *shortcut,
                      const char *data,
                      const char *description);

const char *
gt_macro_manager_add_empty (GtMacroManager *manager);

void
gt_macro_manager_remove (GtMacroManager *self, const char *id);

void
gt_macro_manager_set (GtMacroManager *manager,
                      const char *id,
                      const char *shortcut,
                      const char *data,
                      const char *description);

void
gt_macro_manager_clear (GtMacroManager *manager);

GtMacro *
gt_macro_manager_get (GtMacroManager *manager, const char *id);

G_END_DECLS
