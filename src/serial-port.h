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

#ifndef SERIAL_PORT_H
#define SERIAL_PORT_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "buffer.h"
#include "file-transfer.h"
#include "term_config.h"

#include <glib.h>
#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GT_TYPE_SERIAL_PORT gt_serial_port_get_type ()
G_DECLARE_FINAL_TYPE (GtSerialPort, gt_serial_port, GT, SERIAL_PORT, GObject)

typedef struct _GtSerialPort GtSerialPort;

typedef enum _GtSerialPortState {
    GT_SERIAL_PORT_STATE_ONLINE,
    GT_SERIAL_PORT_STATE_OFFLINE,
    GT_SERIAL_PORT_STATE_ERROR
} GtSerialPortState;

GtSerialPort *gt_serial_port_new (void);

int gt_serial_port_send_chars (GtSerialPort *, char *, int);
gboolean gt_serial_port_config (GtSerialPort *, GtSerialPortConfiguration *config);
void gt_serial_port_set_signals (GtSerialPort *, guint);
guint gt_serial_port_get_signals (GtSerialPort *);
void gt_serial_port_close_and_unlock (GtSerialPort *);
void gt_serial_port_set_local_echo (GtSerialPort *, gboolean);
gboolean gt_serial_port_get_local_echo (GtSerialPort *);
void gt_serial_port_set_crlfauto (GtSerialPort *, gboolean);
gboolean gt_serial_port_get_crlfauto (GtSerialPort *self);
void gt_serial_port_send_brk (GtSerialPort *);
void gt_serial_port_set_custom_speed (GtSerialPort *, int);
gchar *gt_serial_port_to_string (GtSerialPort *);
GError *gt_serial_port_get_last_error (GtSerialPort *self);
GtSerialPortState gt_serial_port_get_status (GtSerialPort *self);
gboolean gt_serial_port_reconnect (GtSerialPort *);
gboolean gt_serial_port_connect (GtSerialPort *self);
GList *gt_serial_port_detect_devices (void);

void
gt_serial_port_write_bytes_async (GtSerialPort *self,
                                  GBytes *bytes,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data);
gsize
gt_serial_port_write_bytes_finish (GtSerialPort *self,
                                   GAsyncResult *result,
                                   GError **error);

GtSerialPortParity
gt_serial_port_parity_from_string (const char *name);

GtSerialPortFlowControl
gt_serial_port_flow_control_from_string (const char *name);

GtFileTransfer *
gt_serial_port_send_file (GtSerialPort *self, GFile *file);

#define LINE_FEED 0x0A

G_END_DECLS

#endif /* SERIAL_PORT_H */
