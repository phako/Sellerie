// SPDX-License-Identifier: GPL-3.0-or-later

#include <config.h>

#include "macro-manager.h"

#include <gtk/gtk.h>

#include <memory.h>
#include <stdio.h>

struct _GtMacro {
    GObject parent_class;
    char *shortcut;
    char *description;
    char *action;
    char *id;
    GBytes *data;
};


static void
gt_macro_finalize (GObject *object);

static void
gt_macro_class_init (GtMacroClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = gt_macro_finalize;
}

static void
gt_macro_init (GtMacro *self)
{
}

G_DEFINE_TYPE (GtMacro, gt_macro, G_TYPE_OBJECT)

static void
gt_macro_finalize (GObject *object)
{
    GtMacro *self = GT_MACRO (object);

    g_clear_pointer (&self->data, g_bytes_unref);
    g_clear_pointer (&self->shortcut, g_free);
    g_clear_pointer (&self->id, g_free);
    g_clear_pointer (&self->action, g_free);
    g_clear_pointer (&self->description, g_free);

    G_OBJECT_CLASS (gt_macro_parent_class)->finalize (object);
}

static GBytes *
gt_macro_parse_data (const char *string)
{
    size_t length = strlen (string);
    const char *str;
    guchar a;
    guint val_read;

    // We will have at most as many bytes as in data if all is plain ASCII
    GByteArray *bytes = g_byte_array_sized_new (length);
    for (size_t i = 0; i < length; i++) {
        // Not escaped, pass on byte as-is
        if (string[i] != '\\') {
            g_byte_array_append (bytes, (const guint8 *)&string[i], 1);
            continue;
        }

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
        g_byte_array_append (bytes, (const guint8 *)&a, 1);
    }

    return g_byte_array_free_to_bytes (bytes);
}

static gboolean
gt_macro_has_id (GtMacro *self, GtMacro *other)
{
    return g_str_equal (self->id, other->id);
}

char *
gt_macro_to_string (GtMacro *self)
{
    return g_strconcat (self->shortcut, "::", self->action, NULL);
}

const char *
gt_macro_get_action (GtMacro *self)
{
    return self->action;
}

const char *
gt_macro_get_shortcut (GtMacro *self)
{
    return self->shortcut;
}

const char *
gt_macro_get_description (GtMacro *self)
{
    return self->description;
}

GBytes *
gt_macro_get_bytes (GtMacro *self)
{
    return self->data;
}

struct _GtMacroManager {
    GObject parent_class;

    GListStore *model;
    GtkApplication *app;
};

enum GtMacroManagerProperties { PROP_APP = 1, N_PROPS };

static GParamSpec *gt_macro_manager_properties[N_PROPS] = {
    NULL,
};

static void
gt_macro_manager_dispose (GObject *object);

static void
gt_macro_manager_set_property (GObject *,
                               guint,
                               const GValue *,
                               GParamSpec *pspec);
static void
gt_macro_manager_get_property (GObject *, guint, GValue *, GParamSpec *pspec);

static void
gt_macro_manager_class_init (GtMacroManagerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->set_property = gt_macro_manager_set_property;
    object_class->get_property = gt_macro_manager_get_property;
    object_class->dispose = gt_macro_manager_dispose;

    gt_macro_manager_properties[PROP_APP] = g_param_spec_object (
        "app",
        "app",
        "app",
        GTK_TYPE_APPLICATION,
        G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (
        object_class, N_PROPS, gt_macro_manager_properties);

}

static void
gt_macro_manager_init (GtMacroManager *self)
{
    self->model = g_list_store_new (GT_TYPE_MACRO);
}

G_DEFINE_TYPE (GtMacroManager, gt_macro_manager, G_TYPE_OBJECT)

static void
gt_macro_manager_dispose (GObject *object)
{
    GtMacroManager *self = GT_MACRO_MANAGER (object);

    g_object_remove_weak_pointer (G_OBJECT (self->app), (gpointer *)&self->app);
    self->app = NULL;
    g_clear_object (&self->model);
    g_clear_object (&self->app);
    G_OBJECT_CLASS (gt_macro_manager_parent_class)->dispose (object);
}

static void
gt_macro_manager_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
    GtMacroManager *self = GT_MACRO_MANAGER (object);

    switch (property_id) {
    case PROP_APP:
        self->app = g_value_get_object (value);
        if (self->app != NULL)
            g_object_add_weak_pointer (G_OBJECT (self->app), (gpointer *) &self->app);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gt_macro_manager_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
    GtMacroManager *self = GT_MACRO_MANAGER (object);

    switch (property_id) {
    case PROP_APP:
        g_value_set_object (value, self->app);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

GtMacroManager *
gt_macro_manager_get_default ()
{
    static GtMacroManager *manager = NULL;

    if (manager == NULL)
        manager = gt_macro_manager_new (NULL);

    return manager;
}

GtMacroManager *
gt_macro_manager_new (GApplication *app)
{
    return g_object_new (GT_TYPE_MACRO_MANAGER, "app", app, NULL);
}

GListModel *
gt_macro_manager_get_model (GtMacroManager *self)
{
    return G_LIST_MODEL (self->model);
}

const char *
gt_macro_manager_add_empty (GtMacroManager *self)
{
    GtMacro *macro = g_object_new (GT_TYPE_MACRO, NULL);
    macro->id = g_uuid_string_random ();

    g_list_store_append (self->model, macro);

    g_autofree char *action = g_strconcat ("main.macro::", macro->id, NULL);
    const char *shortcuts[] = {macro->shortcut, NULL};

    gtk_application_set_accels_for_action (self->app, action, shortcuts);

    return macro->id;
}

const char *
gt_macro_manager_add (GtMacroManager *self,
                      const char *shortcut,
                      const char *data,
                      const char *description)
{
    GtMacro *macro = g_object_new (GT_TYPE_MACRO, NULL);
    macro->id = g_uuid_string_random ();
    macro->shortcut = g_strdup (shortcut);
    macro->description = g_strdup (description);
    macro->action = g_strdup (data);
    macro->data = gt_macro_parse_data (data);

    g_list_store_append (self->model, macro);

    g_autofree char *action = g_strconcat ("main.macro::", macro->id, NULL);
    const char *shortcuts[] = {macro->shortcut, NULL};

    gtk_application_set_accels_for_action (self->app, action, shortcuts);

    return macro->id;
}

const char *
gt_macro_manager_add_from_string (GtMacroManager *self,
                                  const char *macro_string)
{

    char **parts = g_strsplit (macro_string, "::", 2);
    if (g_strv_length (parts) != 2) {
        g_warning ("Failed to parse macro \"%s\"", macro_string);
        g_strfreev (parts);
        return NULL;
    }

    GtMacro *macro = g_object_new (GT_TYPE_MACRO, NULL);
    macro->id = g_uuid_string_random ();

    macro->shortcut = parts[0];
    macro->description = g_strdup ("");
    macro->action = parts[1];
    macro->data = gt_macro_parse_data (parts[1]);

    g_free (parts);

    g_list_store_append (self->model, macro);

    g_autofree char *action = g_strconcat ("main.macro::", macro->id, NULL);
    const char *shortcuts[] = {macro->shortcut, NULL};

    gtk_application_set_accels_for_action (self->app, action, shortcuts);

    return macro->id;
}

void
gt_macro_manager_remove (GtMacroManager *self, const char *id)
{
    guint position;
    g_autoptr (GtMacro) macro = g_object_new (GT_TYPE_MACRO, NULL);
    macro->id = id;

    if (g_list_store_find_with_equal_func (
            self->model, macro, (GEqualFunc)gt_macro_has_id, &position)) {
        g_list_store_remove (self->model, position);
    }

    macro->id = NULL;
}

void
gt_macro_manager_clear (GtMacroManager *self)
{
    while (g_list_model_get_n_items (G_LIST_MODEL (self->model)) > 0) {
        g_autoptr (GtMacro) macro =
            g_list_model_get_item (G_LIST_MODEL (self->model), 0);
        g_autofree char *action = g_strconcat ("main.macro::", macro->id, NULL);
        const char *accels[] = {NULL};
        gtk_application_set_accels_for_action(self->app, action, accels);
        g_list_store_remove (self->model, 0);
    }
}

GtMacro *
gt_macro_manager_get (GtMacroManager *self, const char *id)
{
    guint position;

    g_print ("Trying to find macro with id %s\n", id);
    g_autoptr (GtMacro) macro = g_object_new (GT_TYPE_MACRO, NULL);
    macro->id = id;

    if (g_list_store_find_with_equal_func (self->model,
                                           macro,
                                           (GEqualFunc)gt_macro_has_id,
                                           &position)) {
        macro->id = NULL;
        return g_list_model_get_item (G_LIST_MODEL (self->model), position);
    }

    macro->id = NULL;

    return NULL;
}
