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

#include "infobar.h"

struct _GtInfobar
{
    GtkInfoBar parent_instance;
    GtkProgressBar *progress;
    GtkLabel *label;
};

G_DEFINE_TYPE (GtInfobar, gt_infobar, GTK_TYPE_INFO_BAR)

enum {
    PROP_0,
    N_PROPS
};

//static GParamSpec *properties [N_PROPS];

GtkWidget *
gt_infobar_new (void)
{
    return GTK_WIDGET (g_object_new (GT_TYPE_INFOBAR, NULL));
}

static void
gt_infobar_finalize (GObject *object)
{
    //GtInfobar *self = (GtInfobar *)object;

    G_OBJECT_CLASS (gt_infobar_parent_class)->finalize (object);
}

static void
gt_infobar_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
    //GtInfobar *self = GT_INFOBAR (object);

    switch (prop_id)
      {
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      }
}

static void
gt_infobar_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
    //GtInfobar *self = GT_INFOBAR (object);

    switch (prop_id)
      {
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      }
}

static void
gt_infobar_class_init (GtInfobarClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/jensge/Sellerie/transfer-infobar.ui");
    gtk_widget_class_bind_template_child (widget_class, GtInfobar, progress);
    gtk_widget_class_bind_template_child (widget_class, GtInfobar, label);

    object_class->finalize = gt_infobar_finalize;
    object_class->get_property = gt_infobar_get_property;
    object_class->set_property = gt_infobar_set_property;
}

static void
gt_infobar_init (GtInfobar *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}

void
gt_infobar_set_progress (GtInfobar *self, gdouble fraction)
{
    gtk_progress_bar_set_fraction (self->progress, fraction);
}

void
gt_infobar_set_label (GtInfobar *self, const char *message)
{
    gtk_label_set_text (self->label, message);
}
