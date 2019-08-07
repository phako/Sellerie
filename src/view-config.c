/* view-config.c
 *
 * Copyright (C) 2019 Jens Georg <mail@jensge.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "view-config.h"
#include "util.h"

static const char GT_VIEW_CONFIG_RESOURCE[] =
    "/org/jensge/Sellerie/settings-window.ui";

struct _GtViewConfig {
    GtkDialog parent_object;
    GtkWidget *font_button;
    GtkWidget *color_button_fg;
    GtkWidget *color_button_bg;
    GtkWidget *spin_scrollback;
    GtSerialView *view;
};

struct _GtViewConfigClass {
    GtkDialogClass parent_class;
};

G_DEFINE_TYPE (GtViewConfig, gt_view_config, GTK_TYPE_DIALOG)

enum GtViewConfigProperties { PROP_0, PROP_VIEW, N_PROPERTIES };

static GParamSpec *gt_view_config_properties[N_PROPERTIES] = {NULL};

int
gt_get_use_header_bar (void)
{
    const char *env = g_getenv ("SELLERIE_USE_HEADER_BAR");
    if (env != NULL) {
        return atoi (env);
    }

    GtkSettings *settings = gtk_settings_get_default ();
    if (settings == NULL) {
        return 0;
    }

    gboolean use_header = FALSE;
    g_object_get (
        G_OBJECT (settings), "gtk-dialogs-use-header", &use_header, NULL);

    return use_header ? 1 : 0;
}

GtkWidget *
gt_view_config_new (GtSerialView *view)
{
    return g_object_new (GT_TYPE_VIEW_CONFIG,
                         "view",
                         view,
                         "use-header-bar",
                         gt_get_use_header_bar (),
                         NULL);
}

// GObject class methods

static void
gt_view_config_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GtViewConfig *self = GT_VIEW_CONFIG (object);

    switch (property_id) {
    case PROP_VIEW:
        self->view = g_value_get_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gt_view_config_constructed (GObject *object)
{
    GtViewConfig *self = GT_VIEW_CONFIG (object);
    G_OBJECT_CLASS (gt_view_config_parent_class)->constructed (object);

    // Binding from view to dialog to be able to set the inital values from the
    // view.
    g_object_bind_property (self->view,
                            "font-desc",
                            self->font_button,
                            "font-desc",
                            G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
    g_object_bind_property (self->view,
                            "text",
                            self->color_button_fg,
                            "rgba",
                            G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
    g_object_bind_property (self->view,
                            "background",
                            self->color_button_bg,
                            "rgba",
                            G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

    GtkAdjustment *adjustment = gtk_spin_button_get_adjustment (
        GTK_SPIN_BUTTON (self->spin_scrollback));
    g_object_bind_property (self->view,
                            "scrollback-lines",
                            adjustment,
                            "value",
                            G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
}

static void
gt_view_config_class_init (GtViewConfigClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->set_property = gt_view_config_set_property;
    object_class->constructed = gt_view_config_constructed;

    gt_view_config_properties[PROP_VIEW] = g_param_spec_object (
        "view",
        "view",
        "view",
        GT_TYPE_SERIAL_VIEW,
        G_PARAM_STATIC_STRINGS | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties (
        object_class, N_PROPERTIES, gt_view_config_properties);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 GT_VIEW_CONFIG_RESOURCE);
    gtk_widget_class_bind_template_child (
        widget_class, GtViewConfig, font_button);
    gtk_widget_class_bind_template_child (
        widget_class, GtViewConfig, color_button_fg);
    gtk_widget_class_bind_template_child (
        widget_class, GtViewConfig, color_button_bg);
    gtk_widget_class_bind_template_child (
        widget_class, GtViewConfig, spin_scrollback);
}

static void
gt_view_config_init (GtViewConfig *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}
