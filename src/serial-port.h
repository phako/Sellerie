#ifndef SERIAL_PORT_H
#define SERIAL_PORT_H

#include "term_config.h"
#include "buffer.h"

#include <glib.h>
#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GT_TYPE_SERIAL_PORT gt_serial_port_get_type ()

G_DECLARE_FINAL_TYPE (GtSerialPort, gt_serial_port, GT, SERIAL_PORT, GObject)

typedef struct _GtSerialPort GtSerialPort;

enum _GtSerialPortState {
    GT_SERIAL_PORT_STATE_ONLINE,
    GT_SERIAL_PORT_STATE_OFFLINE,
    GT_SERIAL_PORT_STATE_ERROR
};
typedef enum _GtSerialPortState GtSerialPortState;

GtSerialPort *gt_serial_port_new (void);

void gt_serial_port_set_buffer (GtSerialPort *, GtBuffer *);
int gt_serial_port_send_chars (GtSerialPort *, char *, int);
gboolean gt_serial_port_config (GtSerialPort *, GtSerialPortConfiguration *config);
void gt_serial_port_set_signals (GtSerialPort *, guint);
guint gt_serial_port_get_signals (GtSerialPort *);
void gt_serial_port_close_and_unlock (GtSerialPort *);
void gt_serial_port_set_local_echo (GtSerialPort *, gboolean);
void gt_serial_port_set_crlfauto (GtSerialPort *, gboolean);
void gt_serial_port_send_brk (GtSerialPort *);
void gt_serial_port_set_custom_speed (GtSerialPort *, int);
gchar *gt_serial_port_to_string (GtSerialPort *);
gint gt_serial_port_get_fd (GtSerialPort *);
GError *gt_serial_port_get_last_error (GtSerialPort *self);
GtSerialPortState gt_serial_port_get_status (GtSerialPort *self);
gboolean gt_serial_port_reconnect (GtSerialPort *);
gboolean gt_serial_port_connect (GtSerialPort *self);
GtBuffer *gt_serial_port_get_buffer (GtSerialPort *self);
GList *gt_serial_port_detect_devices (void);

#define BUFFER_RECEPTION 8192
#define BUFFER_EMISSION 4096
#define LINE_FEED 0x0A

G_END_DECLS

#endif /* SERIAL_PORT_H */
