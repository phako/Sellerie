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

#include "file-transfer.h"
#include "serial-port.h"

#include <glib/gi18n.h>

struct _GtFileTransfer {
    GObject parent_instance;
    GFile *file;
    GtSerialPort *port;
    GFileInputStream *stream;
    guint8 buffer[8192];
    gsize size;
    gsize written;
};

G_DEFINE_TYPE (GtFileTransfer, gt_file_transfer, G_TYPE_OBJECT)

enum { PROP_0, PROP_FILE, PROP_SERIAL_PORT, PROP_PROGRESS, N_PROPS };

enum { SIGNAL_PROGRESS, N_SIGNALS };

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS] = {0};

static void
gt_file_transfer_finalize (GObject *object)
{
    GtFileTransfer *self = (GtFileTransfer *)object;

    g_clear_object (&self->stream);
    g_clear_object (&self->port);
    g_clear_object (&self->file);

    G_OBJECT_CLASS (gt_file_transfer_parent_class)->finalize (object);
}

static void
gt_file_transfer_get_property (GObject *object,
                               guint prop_id,
                               GValue *value,
                               GParamSpec *pspec)
{
    GtFileTransfer *self = GT_FILE_TRANSFER (object);

    switch (prop_id) {
    case PROP_PROGRESS:
        if (self->size != 0)
            g_value_set_double (value,
                                (double)self->written / (double)self->size);
        else
            g_value_set_double (value, 0.0);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gt_file_transfer_set_property (GObject *object,
                               guint prop_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
    GtFileTransfer *self = GT_FILE_TRANSFER (object);

    switch (prop_id) {
    case PROP_FILE:
        self->file = G_FILE (g_value_dup_object (value));
        break;
    case PROP_SERIAL_PORT:
        self->port = GT_SERIAL_PORT (g_value_dup_object (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gt_file_transfer_class_init (GtFileTransferClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gt_file_transfer_finalize;
    object_class->get_property = gt_file_transfer_get_property;
    object_class->set_property = gt_file_transfer_set_property;

    signals[SIGNAL_PROGRESS] = g_signal_new ("progress",
                                             G_TYPE_FROM_CLASS (klass),
                                             G_SIGNAL_RUN_LAST,
                                             0,
                                             NULL,
                                             NULL,
                                             g_cclosure_marshal_generic,
                                             G_TYPE_NONE,
                                             1,
                                             G_TYPE_DOUBLE);

    properties[PROP_FILE] = g_param_spec_object (
        "file",
        "file",
        "file",
        G_TYPE_FILE,
        G_PARAM_STATIC_STRINGS | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
    properties[PROP_SERIAL_PORT] = g_param_spec_object (
        "serial-port",
        "serial-port",
        "serial-port",
        GT_TYPE_SERIAL_PORT,
        G_PARAM_STATIC_STRINGS | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
    properties[PROP_PROGRESS] =
        g_param_spec_double ("progress",
                             "progress",
                             "progress",
                             0.0,
                             1.0,
                             0.0,
                             G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gt_file_transfer_init (GtFileTransfer *self)
{
    self->size = 0;
}

// internal functions

static void
on_file_input_ready (GObject *source, GAsyncResult *res, gpointer user_data);

static void
on_serial_port_write_ready (GObject *source,
                            GAsyncResult *res,
                            gpointer user_data)
{
    GTask *task = G_TASK (user_data);
    GError *error = NULL;
    GtFileTransfer *self = GT_FILE_TRANSFER (g_task_get_source_object (task));

    gsize size =
        gt_serial_port_write_finish (GT_SERIAL_PORT (source), res, &error);

    if (error != NULL) {
        g_task_return_error (task, error);
        g_object_unref (G_OBJECT (task));
        return;
    }

    if (size == 0) {
        g_task_return_new_error (
            task,
            G_IO_ERROR,
            G_IO_ERROR_UNKNOWN,
            _ ("Failed to write anything to the serial port"));
        g_object_unref (G_OBJECT (task));
        return;
    }

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PROGRESS]);

    g_input_stream_read_async (G_INPUT_STREAM (self->stream),
                               self->buffer,
                               sizeof (self->buffer),
                               G_PRIORITY_DEFAULT,
                               g_task_get_cancellable (task),
                               on_file_input_ready,
                               task);
}

static void
on_file_input_ready (GObject *source, GAsyncResult *res, gpointer user_data)
{
    GTask *task = G_TASK (user_data);
    GError *error = NULL;
    GtFileTransfer *self = GT_FILE_TRANSFER (g_task_get_source_object (task));

    gssize size =
        g_input_stream_read_finish (G_INPUT_STREAM (source), res, &error);

    if (error != NULL) {
        g_task_return_error (task, error);
        g_object_unref (G_OBJECT (task));
        return;
    }

    // Reading done
    if (size == 0) {
        g_task_return_boolean (task, TRUE);
        g_object_unref (G_OBJECT (task));
        return;
    }

    self->written += size;
    gt_serial_port_write_async (self->port,
                                self->buffer,
                                (gsize)size,
                                g_task_get_cancellable (task),
                                on_serial_port_write_ready,
                                task);
}

static void
on_read_file_done (GObject *source, GAsyncResult *res, gpointer user_data)
{
    GTask *task = G_TASK (user_data);
    GError *error = NULL;
    GtFileTransfer *self = GT_FILE_TRANSFER (g_task_get_source_object (task));
    self->stream = g_file_read_finish (G_FILE (source), res, &error);

    if (error != NULL) {
        g_task_return_error (task, error);
        g_object_unref (G_OBJECT (task));
        return;
    }

    g_input_stream_read_async (G_INPUT_STREAM (self->stream),
                               self->buffer,
                               sizeof (self->buffer),
                               G_PRIORITY_DEFAULT,
                               g_task_get_cancellable (task),
                               on_file_input_ready,
                               task);
}

static void
on_file_info_done (GObject *source, GAsyncResult *res, gpointer user_data)
{
    GTask *task = G_TASK (user_data);
    GError *error = NULL;
    GFileInfo *info = g_file_query_info_finish (G_FILE (source), res, &error);

    if (info == NULL) {
        g_task_return_error (task, error);
        g_object_unref (G_OBJECT (task));
        return;
    }

    GtFileTransfer *self = GT_FILE_TRANSFER (g_task_get_source_object (task));
    self->size = g_file_info_get_size (info);
    g_object_unref (info);

    g_file_read_async (self->file,
                       G_PRIORITY_DEFAULT,
                       g_task_get_cancellable (task),
                       on_read_file_done,
                       task);
}

void
gt_file_transfer_start (GtFileTransfer *self,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
    GTask *task = g_task_new (self, cancellable, callback, user_data);
    g_file_query_info_async (self->file,
                             G_FILE_ATTRIBUTE_STANDARD_SIZE,
                             G_FILE_QUERY_INFO_NONE,
                             G_PRIORITY_DEFAULT,
                             cancellable,
                             on_file_info_done,
                             task);

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PROGRESS]);
}

gboolean
gt_file_transfer_finish (GtFileTransfer *self,
                         GAsyncResult *res,
                         GError **error)
{
    g_return_val_if_fail (g_task_is_valid (G_TASK (res), self), FALSE);

    return g_task_propagate_boolean (G_TASK (res), error);
}
