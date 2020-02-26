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

#define FILE_TRANSFER_BUFFER_SIZE 8192

typedef struct {
    GSource source;

    int wait_char;
    gpointer object;
    guint callback;

    gboolean wait_char_found;
} SignalWaitSource;

static gboolean
signal_wait_source_prepare (GSource *source, gint *timeout)
{
    SignalWaitSource *self = (SignalWaitSource *)source;
    *timeout = -1;

    return self->wait_char_found;
}

static gboolean
signal_wait_source_dispatch (GSource *source,
                             GSourceFunc callback,
                             gpointer user_data)
{
    return callback (user_data);
}

static void
signal_wait_source_finalize (GSource *source)
{
    SignalWaitSource *self = (SignalWaitSource *)source;

    g_signal_handler_disconnect (self->object, self->callback);
}

static GSourceFuncs signal_wait_source_funcs = {signal_wait_source_prepare,
                                                NULL,
                                                signal_wait_source_dispatch,
                                                signal_wait_source_finalize,
                                                NULL,
                                                NULL};

struct _GtFileTransfer {
    GObject parent_instance;
    GFile *file;
    GtSerialPort *port;
    GFileInputStream *stream;
    gsize size;
    gsize written;
    gint wait_character;
    guint wait_delay;
    gulong callback;
    gulong timeout_source;

    gboolean waiting;
    GBytes *residual;
};

G_DEFINE_TYPE (GtFileTransfer, gt_file_transfer, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_FILE,
    PROP_SERIAL_PORT,
    PROP_PROGRESS,
    PROP_WAIT_CHARACTER,
    PROP_DELAY,
    N_PROPS
};

enum { SIGNAL_PROGRESS, N_SIGNALS };

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS] = {0};

static void
gt_file_transfer_send_chunk (GtFileTransfer *self,
                             GBytes *data,
                             gpointer user_data);

static void
gt_file_transfer_continue (GtFileTransfer *self, gpointer user_data);

static void
on_serial_data_ready (GtSerialPort *port, GBytes *data, gpointer user_data);

static void
gt_file_transfer_dispose (GObject *object)
{
    GtFileTransfer *self = (GtFileTransfer *)object;

    if (self->callback != 0) {
        g_signal_handler_disconnect (self->port, self->callback);
        self->callback = 0;
    }

    G_OBJECT_CLASS (gt_file_transfer_parent_class)->dispose (object);
}

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
    case PROP_WAIT_CHARACTER:
        self->wait_character = g_value_get_int (value);
        break;
    case PROP_DELAY:
        self->wait_delay = g_value_get_uint (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gt_file_transfer_class_init (GtFileTransferClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = gt_file_transfer_dispose;
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

    properties[PROP_WAIT_CHARACTER] = g_param_spec_int (
        "wait-character",
        "wait-character",
        "wait-character",
        -1,
        G_MAXINT,
        -1,
        G_PARAM_STATIC_STRINGS | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

    properties[PROP_DELAY] = g_param_spec_uint (
        "delay",
        "delay",
        "delay",
        0,
        G_MAXUINT,
        0,
        G_PARAM_STATIC_STRINGS | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

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

static gboolean
on_delay_timeout (gpointer user_data)
{
    GTask *task = G_TASK (user_data);
    GtFileTransfer *self = GT_FILE_TRANSFER (g_task_get_source_object (task));
    self->waiting = FALSE;

    g_debug ("output delay timeout: %lu", g_get_monotonic_time ());

    gt_file_transfer_continue (self, user_data);

    return G_SOURCE_REMOVE;
}

static void
on_serial_port_write_ready (GObject *source,
                            GAsyncResult *res,
                            gpointer user_data)
{
    GTask *task = G_TASK (user_data);
    GError *error = NULL;
    GtFileTransfer *self = GT_FILE_TRANSFER (g_task_get_source_object (task));

    gsize size = gt_serial_port_write_bytes_finish (
        GT_SERIAL_PORT (source), res, &error);

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

    if (!self->waiting) {
        gt_file_transfer_continue (self, task);

        return;
    }

    GSource *wait_source = NULL;
    if (self->wait_character != -1) {
        wait_source =
            g_source_new (&signal_wait_source_funcs, sizeof (SignalWaitSource));
        SignalWaitSource *source = (SignalWaitSource *)wait_source;
        source->wait_char_found = FALSE;
        source->wait_char = self->wait_character;
        source->object = self->port;
        source->callback = g_signal_connect (G_OBJECT (self->port),
                                             "data-available",
                                             G_CALLBACK (on_serial_data_ready),
                                             wait_source);

        g_debug ("=> %u", source->callback);
    } else if (self->wait_delay != 0) {
        wait_source = g_timeout_source_new (self->wait_delay);
    } else {
        g_assert_not_reached ();
    }

    GSource *cancellable_source =
        g_cancellable_source_new (g_task_get_cancellable (task));
    g_source_set_dummy_callback (cancellable_source);
    g_source_add_child_source (wait_source, cancellable_source);
    g_task_attach_source (task, wait_source, on_delay_timeout);
    g_source_unref (wait_source);
    g_source_unref (cancellable_source);
}

static void
gt_file_transfer_continue (GtFileTransfer *self, gpointer user_data)
{
    // We have left-over data from the previous write due to a mode where we
    // have to obey the LF. Short-cut to send the data without reading from the
    // file
    if (self->residual != NULL) {
        GBytes *data = self->residual;
        self->residual = NULL;
        gt_file_transfer_send_chunk (self, data, user_data);

        return;
    }

    g_input_stream_read_bytes_async (
        G_INPUT_STREAM (self->stream),
        FILE_TRANSFER_BUFFER_SIZE,
        g_task_get_priority (G_TASK (user_data)),
        g_task_get_cancellable (G_TASK (user_data)),
        on_file_input_ready,
        user_data);
}

static void
on_file_input_ready (GObject *source, GAsyncResult *res, gpointer user_data)
{
    GTask *task = G_TASK (user_data);
    GError *error = NULL;
    GtFileTransfer *self = GT_FILE_TRANSFER (g_task_get_source_object (task));

    GBytes *data =
        g_input_stream_read_bytes_finish (G_INPUT_STREAM (source), res, &error);

    if (error != NULL) {
        g_task_return_error (task, error);
        g_object_unref (G_OBJECT (task));
        return;
    }

    gt_file_transfer_send_chunk (self, data, user_data);
}

static void
gt_file_transfer_send_chunk (GtFileTransfer *self,
                             GBytes *data,
                             gpointer user_data)
{
    GTask *task = G_TASK (user_data);

    // Reading done
    gsize size = 0;
    const guint8 *bytes = (const guint8 *)g_bytes_get_data (data, &size);
    if (g_bytes_get_size (data) == 0) {
        g_debug ("FInishing task because there's no data left to send");
        g_bytes_unref (data);
        g_task_return_boolean (task, TRUE);
        g_object_unref (G_OBJECT (task));
        return;
    }

    if (self->wait_character != -1 || self->wait_delay != 0) {
        gsize lf_position = 0, old_size = size;
        while (lf_position < size && bytes[lf_position++] != LINE_FEED) {
        }

        if (lf_position + 1 < size) {
            GBytes *old_residual = self->residual;
            self->residual = g_bytes_new_from_bytes (
                data, lf_position + 1, size - lf_position - 1);
            size = lf_position + 1;
            GBytes *old_data = data;
            data = g_bytes_new_from_bytes (data, 0, size);
            g_bytes_unref (old_residual);
            g_bytes_unref (old_data);
        }

        // Check if the buffer ends on a line-feed, so we have to wait for
        // something later on
        self->waiting =
            lf_position < old_size && bytes[lf_position] == LINE_FEED;
    }

    self->written += size;
    gt_serial_port_write_bytes_async (self->port,
                                      data,
                                      g_task_get_cancellable (task),
                                      on_serial_port_write_ready,
                                      task);
    g_bytes_unref (data);
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

    gt_file_transfer_continue (self, user_data);
}

static void
on_serial_data_ready (GtSerialPort *port, GBytes *data, gpointer user_data)
{
    SignalWaitSource *self = (SignalWaitSource *)user_data;

    // Should not happen
    if (self->wait_char == -1) {
        return;
    }

    gsize size = 0;
    const guint8 *bytes = (const guint8 *)g_bytes_get_data (data, &size);
    gssize char_position = 0;
    while (char_position < size) {
        if (bytes[char_position++] == self->wait_char) {
            self->wait_char_found = TRUE;
            break;
        }
    }
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
                       g_task_get_priority (task),
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
    g_autofree char *path = g_file_get_path (self->file);
    g_autofree char *task_name = g_strdup_printf ("Transferring file %s", path);

    g_debug ("Starting file transfer of %s", path);

    GTask *task = g_task_new (self, cancellable, callback, user_data);
    g_task_set_name (task, task_name);
    g_file_query_info_async (self->file,
                             G_FILE_ATTRIBUTE_STANDARD_SIZE,
                             G_FILE_QUERY_INFO_NONE,
                             g_task_get_priority (task),
                             g_task_get_cancellable (task),
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
