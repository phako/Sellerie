#ifndef SERIAL_PORT_H
#define SERIAL_PORT_H

#include <glib.h>
#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GT_TYPE_SERIAL_PORT gt_serial_port_get_type ()

G_DECLARE_FINAL_TYPE (GtSerialPort, gt_serial_port, GT, SERIAL_PORT, GObject)

typedef struct _GtSerialPort GtSerialPort;

GtSerialPort *gt_serial_port_new (void);

int gt_serial_port_send_chars (GtSerialPort *, char *, int);
gboolean gt_serial_port_config (GtSerialPort *);
void gt_serial_port_set_signals (GtSerialPort *, guint);
int gt_serial_port_read_signals (GtSerialPort *);
void gt_serial_port_close_and_unlock (GtSerialPort *);
void gt_serial_port_set_local_echo (GtSerialPort *, gboolean);
void gt_serial_port_set_crlfauto (GtSerialPort *, gboolean);
void gt_serial_port_send_brk (GtSerialPort *);
void gt_serial_port_set_custom_speed (GtSerialPort *, int);
gchar *gt_serial_port_to_string (GtSerialPort *);
gint gt_serial_port_get_fd (GtSerialPort *);

#define BUFFER_RECEPTION 8192
#define BUFFER_EMISSION 4096
#define LINE_FEED 0x0A
#define POLL_DELAY 100               /* in ms (for control signals) */

G_END_DECLS

#endif /* SERIAL_PORT_H */
