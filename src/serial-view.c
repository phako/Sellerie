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

#include <glib-object.h>

struct _GtHexDisplay {
    guint bytes_per_line;
    guint total_bytes;
    gboolean show_index;

    gchar data[128];
    gchar data_byte[6];
    guint bytes;
};
typedef struct _GtHexDisplay GtHexDisplay;

typedef struct {
    GtSerialViewMode mode;
    GtBuffer *buffer;
    GtHexDisplay hex_display;
    GdkRGBA *text;
    GdkRGBA *background;
} GtSerialViewPrivate;

struct _GtSerialView {
    VteTerminal parent_object;
};

struct _GtSerialViewClass {
    VteTerminalClass parent_class;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtSerialView, gt_serial_view, VTE_TYPE_TERMINAL)

enum { PROP_0, PROP_BUFFER, PROP_TEXT, PROP_BACKGROUND, N_PROPS };
static GParamSpec *properties[N_PROPS] = {NULL};

enum { SIGNAL_NEW_DATA, SIGNAL_COUNT };
static guint SIGNALS[SIGNAL_COUNT] = {0};

void
on_write_hex (GtSerialView *self, gchar *string, guint size);

void
on_write_ascii (GtSerialView *self, gchar *string, guint size);

static void
on_buffer_updated (GtSerialView *self,
                   gpointer data,
                   guint size,
                   gpointer user_data)
{
    GtSerialViewPrivate *priv = gt_serial_view_get_instance_private (self);
    if (priv->mode == GT_SERIAL_VIEW_HEX)
        on_write_hex (self, (gchar *)data, size);
    else
        on_write_ascii (self, (gchar *)data, size);
}

GtkWidget *
gt_serial_view_new (GtBuffer *buffer)
{
    return g_object_new (GT_TYPE_SERIAL_VIEW,
                         "buffer",
                         buffer,
                         "scroll-on-keystroke",
                         TRUE,
                         "scroll-on-output",
                         FALSE,
                         "pointer-autohide",
                         TRUE,
                         "backspace-binding",
                         VTE_ERASE_ASCII_BACKSPACE,
                         NULL);
}

static void
gt_serial_view_finalize (GObject *object)
{
    GtSerialView *self = (GtSerialView *)object;
    GtSerialViewPrivate *priv = gt_serial_view_get_instance_private (self);

    g_clear_object (&priv->buffer);

    G_OBJECT_CLASS (gt_serial_view_parent_class)->finalize (object);
}

static void
gt_serial_view_get_property (GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    GtSerialView *self = GT_SERIAL_VIEW (object);
    GtSerialViewPrivate *priv = gt_serial_view_get_instance_private (self);

    switch (prop_id) {
    case PROP_BUFFER:
        g_value_set_object (value, priv->buffer);
        break;
    case PROP_TEXT:
        g_value_set_boxed (value, priv->text);
        break;
    case PROP_BACKGROUND:
        g_value_set_boxed (value, priv->background);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gt_serial_view_set_property (GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GtSerialView *self = GT_SERIAL_VIEW (object);
    GtSerialViewPrivate *priv = gt_serial_view_get_instance_private (self);

    switch (prop_id) {
    case PROP_BUFFER:
        priv->buffer = g_value_dup_object (value);
        break;
    case PROP_TEXT:
        gt_serial_view_set_text_color (self, g_value_get_boxed (value));
        break;
    case PROP_BACKGROUND:
        gt_serial_view_set_background_color (self, g_value_get_boxed (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gt_serial_view_constructed (GObject *object)
{
    GtSerialView *self = GT_SERIAL_VIEW (object);
    GtSerialViewPrivate *priv = gt_serial_view_get_instance_private (self);

    g_signal_connect_swapped (
        priv->buffer, "cleared", G_CALLBACK (gt_serial_view_clear), self);
    g_signal_connect_swapped (
        priv->buffer, "buffer-updated", G_CALLBACK (on_buffer_updated), self);
    G_OBJECT_CLASS (gt_serial_view_parent_class)->constructed (object);
}

static void
gt_serial_view_class_init (GtSerialViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->constructed = gt_serial_view_constructed;
    object_class->finalize = gt_serial_view_finalize;
    object_class->get_property = gt_serial_view_get_property;
    object_class->set_property = gt_serial_view_set_property;

    SIGNALS[SIGNAL_NEW_DATA] = g_signal_new ("updated",
                                             GT_TYPE_SERIAL_VIEW,
                                             G_SIGNAL_RUN_FIRST,
                                             0,
                                             NULL,
                                             NULL,
                                             NULL,
                                             G_TYPE_NONE,
                                             2,
                                             G_TYPE_STRING,
                                             G_TYPE_UINT64);

    properties[PROP_BUFFER] = g_param_spec_object (
        "buffer",
        "buffer",
        "buffer",
        GT_TYPE_BUFFER,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    properties[PROP_TEXT] = g_param_spec_boxed (
        "text",
        "text",
        "text",
        GDK_TYPE_RGBA,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS |
            G_PARAM_EXPLICIT_NOTIFY);

    properties[PROP_BACKGROUND] = g_param_spec_boxed (
        "background",
        "background",
        "background",
        GDK_TYPE_RGBA,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS |
            G_PARAM_EXPLICIT_NOTIFY);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gt_serial_view_init (GtSerialView *self)
{
    GtSerialViewPrivate *priv = gt_serial_view_get_instance_private (self);

    priv->mode = GT_SERIAL_VIEW_TEXT;
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
gt_serial_view_set_show_index (GtSerialView *self, gboolean show)
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
gt_serial_view_set_bytes_per_line (GtSerialView *self, guint bytes_per_line)
{
    GtSerialViewPrivate *priv = gt_serial_view_get_instance_private (self);

    priv->hex_display.bytes_per_line = bytes_per_line;
}

void
gt_serial_view_set_display_mode (GtSerialView *self, GtSerialViewMode mode)
{
    GtSerialViewPrivate *priv = gt_serial_view_get_instance_private (self);
    gt_serial_view_clear (self);

    priv->mode = mode;

    // Trigger re-display
    gt_buffer_write (priv->buffer);
}

void
gt_serial_view_set_text_color (GtSerialView *self, const GdkRGBA *text)
{
    GtSerialViewPrivate *priv = gt_serial_view_get_instance_private (self);

    g_clear_pointer (&priv->text, gdk_rgba_free);
    priv->text = (text == NULL ? NULL : gdk_rgba_copy (text));
    if (priv->text != NULL)
        vte_terminal_set_color_foreground (VTE_TERMINAL (self), priv->text);

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TEXT]);
}

void
gt_serial_view_set_background_color (GtSerialView *self,
                                     const GdkRGBA *background)
{
    GtSerialViewPrivate *priv = gt_serial_view_get_instance_private (self);

    g_clear_pointer (&priv->background, gdk_rgba_free);
    priv->background = (background == NULL ? NULL : gdk_rgba_copy (background));
    if (priv->background != NULL)
        vte_terminal_set_color_background (VTE_TERMINAL (self),
                                           priv->background);

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BACKGROUND]);
}

void
on_write_hex (GtSerialView *self, gchar *string, guint size)
{
    GtSerialViewPrivate *priv = gt_serial_view_get_instance_private (self);
    VteTerminal *term = VTE_TERMINAL (self);
    GtHexDisplay *display = &(priv->hex_display);
    glong column, row;

    guint i = 0;

    if (size == 0) {
        return;
    }

    // We are spinning the mainloop below. If the user closes the window it
    // might mean that we accidentally access a disposed VTE. So we add a weak
    // pointer here that we can check after the loop iteration.
    // FIXME: Need to do this without the nested main loop (which is ugly)
    g_object_add_weak_pointer (G_OBJECT (self), (gpointer *)&term);

    while (i < size) {
        while (gtk_events_pending ())
            gtk_main_iteration ();

        // User closed window, stop right here
        if (term == NULL)
            return;

        vte_terminal_get_cursor_position (term, &column, &row);

        if (display->show_index) {
            if (column == 0)
            /* First byte on line */
            {
                sprintf (display->data, "%6d: ", display->total_bytes);
                vte_terminal_feed (term, display->data, strlen (display->data));
                display->bytes = 0;
            }
        } else {
            if (column == 0)
                display->bytes = 0;
        }

        /* Print hexadecimal characters */
        display->data[0] = 0;

        guint bytes_per_line = display->bytes_per_line;
        while (display->bytes < bytes_per_line && i < size) {
            gint avance = 0;
            gchar ascii[1];

            sprintf (display->data_byte, "%02X ", (guchar)string[i]);
            g_signal_emit (
                self, SIGNALS[SIGNAL_NEW_DATA], 0, display->data_byte, 3);

            vte_terminal_feed (term, display->data_byte, 3);

            avance = (bytes_per_line - display->bytes) * 3 + display->bytes + 2;

            /* Move forward */
            sprintf (display->data_byte, "%c[%dC", 27, avance);
            vte_terminal_feed (
                term, display->data_byte, strlen (display->data_byte));

            /* Print ascii characters */
            ascii[0] = (string[i] > 0x1F) ? string[i] : '.';
            vte_terminal_feed (term, ascii, 1);

            /* Move backward */
            sprintf (display->data_byte, "%c[%dD", 27, avance + 1);
            vte_terminal_feed (
                term, display->data_byte, strlen (display->data_byte));

            if (display->bytes == bytes_per_line / 2 - 1)
                vte_terminal_feed (term, "- ", strlen ("- "));

            display->bytes++;
            i++;

            /* End of line ? */
            if (display->bytes == bytes_per_line) {
                vte_terminal_feed (term, "\r\n", 2);
                display->total_bytes += display->bytes;
            }
        }
    }

    g_object_remove_weak_pointer (G_OBJECT (self), (gpointer *)&term);
}

void
on_write_ascii (GtSerialView *self, gchar *string, guint size)
{
    vte_terminal_feed (VTE_TERMINAL (self), string, size);
    g_signal_emit (self, SIGNALS[SIGNAL_NEW_DATA], 0, string, size);
}
