/* serial-view.c
 *
 * Copyright (C) 2016 Jens Georg <mail@jensge.org>
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

#include "serial-view.h"

struct _GtHexDisplay {
    guint bytes_per_line;
    guint total_bytes;
    gboolean show_index;
};
typedef struct _GtHexDisplay GtHexDisplay;


typedef struct
{
    GtHexDisplay hex_display;
} GtSerialViewPrivate;

struct _GtSerialView {
    VteTerminal parent_object;
};

struct _GtSerialViewClass {
    VteTerminalClass parent_class;
};


G_DEFINE_TYPE_WITH_PRIVATE (GtSerialView, gt_serial_view, VTE_TYPE_TERMINAL)

/*
enum {
        PROP_0,
        N_PROPS
};

static GParamSpec *properties [N_PROPS];
*/

GtkWidget *
gt_serial_view_new (void)
{
    return g_object_new (GT_TYPE_SERIAL_VIEW,
                          "scroll-on-keystroke", TRUE,
                          "scroll-on-output", FALSE,
                          "pointer-autohide", TRUE,
                          "backspace-binding", VTE_ERASE_ASCII_BACKSPACE,
                          NULL);
}

static void
gt_serial_view_finalize (GObject *object)
{
//        GtSerialView *self = (GtSerialView *)object;
//        GtSerialViewPrivate *priv = gt_serial_view_get_instance_private (self);

        G_OBJECT_CLASS (gt_serial_view_parent_class)->finalize (object);
}

static void
gt_serial_view_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
//        GtSerialView *self = GT_TYPE_SERIAL_VIEW (object);

        switch (prop_id)
          {
          default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gt_serial_view_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
//        GtSerialView *self = GT_TYPE_SERIAL_VIEW (object);

        switch (prop_id)
          {
          default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          }
}

static void
gt_serial_view_class_init (GtSerialViewClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gt_serial_view_finalize;
        object_class->get_property = gt_serial_view_get_property;
        object_class->set_property = gt_serial_view_set_property;
}

static void
gt_serial_view_init (GtSerialView *self)
{
    GtSerialViewPrivate *priv = gt_serial_view_get_instance_private (self);

    priv->hex_display.total_bytes = 0;
    priv->hex_display.bytes_per_line = 16;
    priv->hex_display.show_index = FALSE;
}

void
gt_serial_view_clear (GtSerialView *self)
{
    GtSerialViewPrivate *priv = gt_serial_view_get_instance_private (self);

    priv->hex_display.total_bytes = 0;
    vte_terminal_reset (VTE_TERMINAL (self), TRUE, TRUE);
}

gboolean
gt_serial_view_get_show_index (GtSerialView *self)
{
  GtSerialViewPrivate *priv = gt_serial_view_get_instance_private (self);

  return priv->hex_display.show_index;
}


void
gt_serial_view_set_show_index (GtSerialView *self,
                               gboolean      show)
{
  GtSerialViewPrivate *priv = gt_serial_view_get_instance_private (self);

  priv->hex_display.show_index = show;
}

guint
gt_serial_view_get_bytes_per_line (GtSerialView *self)
{
  GtSerialViewPrivate *priv = gt_serial_view_get_instance_private (self);

  return priv->hex_display.bytes_per_line;
}

void
gt_serial_view_set_bytes_per_line (GtSerialView *self,
                                   guint         bytes_per_line)
{
  GtSerialViewPrivate *priv = gt_serial_view_get_instance_private (self);

  priv->hex_display.bytes_per_line = bytes_per_line;
}

void gt_serial_view_inc_total_bytes (GtSerialView *self,
                                     guint         bytes)
{
  GtSerialViewPrivate *priv = gt_serial_view_get_instance_private (self);

  priv->hex_display.total_bytes += bytes;
}

guint gt_serial_view_get_total_bytes (GtSerialView *self)
{
  GtSerialViewPrivate *priv = gt_serial_view_get_instance_private (self);

  return priv->hex_display.total_bytes;
}

