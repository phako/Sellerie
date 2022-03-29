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

#include "serial-port.h"

#include "buffer.h"
#include "sellerie-enums.h"
#include "term_config.h"
#include "util.h"

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef HAVE_LINUX_SERIAL_H
#include <linux/serial.h>
#endif

#ifdef HAVE_GUDEV
#include <gudev/gudev.h>
#endif

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#define RECEIVE_BUFFER_SIZE 8192

#define GT_SERIAL_PORT_CONTROL_POLL_DELAY                                      \
    100 /* in ms (for control signals)                                         \
           */

typedef struct {
    GOutputStream *output_stream;
    GInputStream *input_stream;
    struct configuration_port config;
    struct termios termios_save;
    int serial_port_fd;
    char lockfile[256];

    GtSerialPortState state;
    GError *last_error;
    int control_flags;
    guint status_timeout;
    GtBuffer *buffer;
    GCancellable *cancellable;
} GtSerialPortPrivate;

struct _GtSerialPort {
    GObject parent_instance;
};

struct _GtSerialPortClass {
    GObjectClass parent_class;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtSerialPort, gt_serial_port, G_TYPE_OBJECT)

enum GtSerialPortProperties {
    PROP_STATUS = 1,
    PROP_LOCAL_ECHO,
    PROP_CRLF,
    PROP_ERROR,
    PROP_CONTROL,
    N_PROPERTIES
};

static GParamSpec *gt_serial_port_properties[N_PROPERTIES] = {
    NULL,
};

enum GtSerialPortSignals {
    SIGNAL_DATA_AVAILABLE,
    SIGNAL_COUNT,
};

static guint SIGNALS[SIGNAL_COUNT] = {0};

/* Local functions prototype */
static gboolean
gt_serial_port_lock (GtSerialPort *, char *, GError **);
static void
gt_serial_port_unlock (GtSerialPort *);
static void
gt_serial_port_close (GtSerialPort *);
static void
gt_serial_port_set_status (GtSerialPort *self,
                           GtSerialPortState state,
                           GError *error);
static gboolean
gt_serial_port_termios_from_config (GtSerialPort *self,
                                    struct termios *xtermios_p,
                                    GError **error);
static gboolean gt_serial_port_on_control_signals_read (gpointer);

static int
gt_serial_port_read_signals (GtSerialPort *self);

static void
gt_serial_port_on_data_ready (GObject *source,
                              GAsyncResult *res,
                              gpointer user_data);

/* GObject overrides */
static void
gt_serial_port_set_property (GObject *,
                             guint,
                             const GValue *,
                             GParamSpec *pspec);
static void
gt_serial_port_get_property (GObject *, guint, GValue *, GParamSpec *pspec);
static void
gt_serial_port_finalize (GObject *object);
static void
gt_serial_port_dispose (GObject *object);

static gboolean
gt_serial_port_termios_from_config (GtSerialPort *self,
                                    struct termios *termios_p,
                                    GError **error)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    switch (priv->config.vitesse) {
    case 300:
        termios_p->c_cflag = B300;
        break;
    case 600:
        termios_p->c_cflag = B600;
        break;
    case 1200:
        termios_p->c_cflag = B1200;
        break;
    case 2400:
        termios_p->c_cflag = B2400;
        break;
    case 4800:
        termios_p->c_cflag = B4800;
        break;
    case 9600:
        termios_p->c_cflag = B9600;
        break;
    case 19200:
        termios_p->c_cflag = B19200;
        break;
    case 38400:
        termios_p->c_cflag = B38400;
        break;
    case 57600:
        termios_p->c_cflag = B57600;
        break;
    case 115200:
        termios_p->c_cflag = B115200;
        break;

    default:
#ifdef HAVE_LINUX_SERIAL_H
        gt_serial_port_set_custom_speed (self, priv->config.vitesse);
        termios_p->c_cflag |= B38400;
#else
        g_propagate_error (
            error,
            g_error_new_literal (G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 _ ("Arbitrary baud rates not supported")));
        return FALSE;
#endif
    }

    switch (priv->config.bits) {
    case 5:
        termios_p->c_cflag |= CS5;
        break;
    case 6:
        termios_p->c_cflag |= CS6;
        break;
    case 7:
        termios_p->c_cflag |= CS7;
        break;
    case 8:
        termios_p->c_cflag |= CS8;
        break;
    default:
        g_assert_not_reached ();
    }

    switch (priv->config.parity) {
    case GT_SERIAL_PORT_PARITY_ODD:
        termios_p->c_cflag |= PARODD | PARENB;
        break;
    case GT_SERIAL_PORT_PARITY_EVEN:
        termios_p->c_cflag |= PARENB;
        break;
    default:
        break;
    }

    if (priv->config.stops == 2)
        termios_p->c_cflag |= CSTOPB;

    termios_p->c_cflag |= CREAD;
    termios_p->c_iflag = IGNPAR | IGNBRK;

    switch (priv->config.flow) {
    case GT_SERIAL_PORT_FLOW_CONTROL_XON:
        termios_p->c_iflag |= IXON | IXOFF;
        break;
    case GT_SERIAL_PORT_FLOW_CONTROL_RTS:
        termios_p->c_cflag |= CRTSCTS;
        break;
    default:
        termios_p->c_cflag |= CLOCAL;
        break;
    }

    termios_p->c_oflag = 0;
    termios_p->c_lflag = 0;
    termios_p->c_cc[VTIME] = 0;
    termios_p->c_cc[VMIN] = 1;

    return TRUE;
}

gsize
gt_serial_port_write (GtSerialPort *self,
                      const char *data,
                      gsize length,
                      GError **error)
{
    return gt_serial_port_send_chars (self, (char *)data, (int)length);
}

int
gt_serial_port_send_chars (GtSerialPort *self, char *string, int length)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);
    int bytes_written = 0;

    if (priv->serial_port_fd == -1)
        return 0;

    /* Normally it never happens, but it is better not to segfault ;) */
    if (length == 0)
        return 0;

    /* RS485 half-duplex mode ? */
    if (priv->config.flow == GT_SERIAL_PORT_FLOW_CONTROL_RS485) {
        /* set RTS (start to send) */
        gt_serial_port_set_signals (self, 1);
        if (priv->config.rs485_rts_time_before_transmit > 0)
            usleep (priv->config.rs485_rts_time_before_transmit * 1000);
    }

    GError *error = NULL;
    bytes_written = g_output_stream_write (
        priv->output_stream, string, length, priv->cancellable, &error);

    if (error != NULL) {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            gt_serial_port_close (self);
            gt_serial_port_set_status (self, GT_SERIAL_PORT_STATE_ERROR, error);
        } else {
            g_clear_error (&error);
        }

        return -1;
    }

    /* RS485 half-duplex mode ? */
    if (priv->config.flow == GT_SERIAL_PORT_FLOW_CONTROL_RS485) {
        /* wait all chars are send */
        tcdrain (priv->serial_port_fd);
        if (priv->config.rs485_rts_time_after_transmit > 0)
            usleep (priv->config.rs485_rts_time_after_transmit * 1000);
        /* reset RTS (end of send, now receiving back) */
        gt_serial_port_set_signals (self, 1);
    }

    return bytes_written;
}

gboolean
gt_serial_port_config (GtSerialPort *self, struct configuration_port *config)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    gt_serial_port_close (self);
    gt_serial_port_unlock (self);

    memcpy (&priv->config, config, sizeof (struct configuration_port));

    return gt_serial_port_connect (self);
}

gboolean
gt_serial_port_reconnect (GtSerialPort *self)
{
    gt_serial_port_close (self);
    gt_serial_port_unlock (self);

    return gt_serial_port_connect (self);
}

gboolean
gt_serial_port_connect (GtSerialPort *self)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);
    struct termios termios_p;
    GError *error = NULL;

    priv->cancellable = g_cancellable_new ();
    priv->serial_port_fd =
        open (priv->config.port, O_RDWR | O_NOCTTY | O_NDELAY);
    if (priv->serial_port_fd == -1) {
        error = g_error_new (G_IO_ERROR,
                             g_io_error_from_errno (errno),
                             _ ("Cannot open %s: %s"),
                             priv->config.port,
                             g_strerror (errno));
        gt_serial_port_set_status (self, GT_SERIAL_PORT_STATE_ERROR, error);

        return FALSE;
    }

    if (!gt_serial_port_lock (self, priv->config.port, &error)) {
        gt_serial_port_close (self);
        gt_serial_port_set_status (self, GT_SERIAL_PORT_STATE_ERROR, error);

        return FALSE;
    }

    tcgetattr (priv->serial_port_fd, &termios_p);
    memcpy (&(priv->termios_save), &termios_p, sizeof (struct termios));

    if (!gt_serial_port_termios_from_config (self, &termios_p, &error)) {
        gt_serial_port_close (self);
        gt_serial_port_set_status (self, GT_SERIAL_PORT_STATE_ERROR, error);
    }

    tcsetattr (priv->serial_port_fd, TCSANOW, &termios_p);
    tcflush (priv->serial_port_fd, TCOFLUSH);
    tcflush (priv->serial_port_fd, TCIFLUSH);

    priv->input_stream = g_unix_input_stream_new (priv->serial_port_fd, FALSE);
    priv->output_stream =
        g_unix_output_stream_new (priv->serial_port_fd, FALSE);

    g_input_stream_read_bytes_async (priv->input_stream,
                                     RECEIVE_BUFFER_SIZE,
                                     G_PRIORITY_DEFAULT,
                                     priv->cancellable,
                                     gt_serial_port_on_data_ready,
                                     self);

    g_object_notify (G_OBJECT (self), "local-echo");

    gt_serial_port_set_status (self, GT_SERIAL_PORT_STATE_ONLINE, NULL);

    priv->status_timeout =
        g_timeout_add (GT_SERIAL_PORT_CONTROL_POLL_DELAY,
                       gt_serial_port_on_control_signals_read,
                       self);

    return TRUE;
}

void
gt_serial_port_set_local_echo (GtSerialPort *self, gboolean echo)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    /* Double book-keeping for now */
    priv->config.echo = echo;
}

gboolean
gt_serial_port_get_local_echo (GtSerialPort *self)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    /* Double book-keeping for now */
    return priv->config.echo;
}

void
gt_serial_port_set_crlfauto (GtSerialPort *self, gboolean crlfauto)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    /* Double book-keeping for now */
    priv->config.crlfauto = crlfauto;
}

gboolean
gt_serial_port_get_crlfauto (GtSerialPort *self)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    /* Double book-keeping for now */
    return priv->config.crlfauto;
}

void
gt_serial_port_close (GtSerialPort *self)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);
    if (priv->serial_port_fd != -1) {
        tcsetattr (priv->serial_port_fd, TCSANOW, &(priv->termios_save));
        tcflush (priv->serial_port_fd, TCOFLUSH);
        tcflush (priv->serial_port_fd, TCIFLUSH);

        if (priv->cancellable != NULL) {
            g_cancellable_cancel (priv->cancellable);
            g_clear_object (&priv->cancellable);
        }

        // TODO: Really ignore errors on close?
        g_output_stream_close (priv->output_stream, NULL, NULL);
        g_input_stream_close (priv->input_stream, NULL, NULL);
        close (priv->serial_port_fd);
        priv->serial_port_fd = -1;
        gt_serial_port_set_status (self, GT_SERIAL_PORT_STATE_OFFLINE, NULL);
    }

    if (priv->status_timeout != 0) {
        g_source_remove (priv->status_timeout);
        priv->status_timeout = 0;
    }
}

void
gt_serial_port_set_signals (GtSerialPort *self, guint param)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);
    int stat_;
    int saved_errno = 0;

    if (priv->serial_port_fd == -1)
        return;

    if (ioctl (priv->serial_port_fd, TIOCMGET, &stat_) == -1) {
        int saved_errno = errno;

        g_critical (_ ("Control signals read: %s"), g_strerror (saved_errno));
        return;
    }

    /* DTR */
    if (param == 0) {
        if (stat_ & TIOCM_DTR)
            stat_ &= ~TIOCM_DTR;
        else
            stat_ |= TIOCM_DTR;
        if (ioctl (priv->serial_port_fd, TIOCMSET, &stat_) == -1) {
            saved_errno = errno;
            g_critical (_ ("DTR write: %s"), g_strerror (saved_errno));
        }
    }
    /* RTS */
    else if (param == 1) {
        if (stat_ & TIOCM_RTS)
            stat_ &= ~TIOCM_RTS;
        else
            stat_ |= TIOCM_RTS;
        if (ioctl (priv->serial_port_fd, TIOCMSET, &stat_) == -1)
            g_critical (_ ("RTS write: %s"), g_strerror (saved_errno));
    }
}

guint
gt_serial_port_get_signals (GtSerialPort *self)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    return priv->control_flags;
}

static int
gt_serial_port_read_signals (GtSerialPort *self)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);
    int stat_read;

    if (priv->config.flow == GT_SERIAL_PORT_FLOW_CONTROL_RS485) {
        /* reset RTS (default = receive) */
        gt_serial_port_set_signals (self, 1);
    }

    if (priv->serial_port_fd != -1 && isatty(priv->serial_port_fd)) {
        if (ioctl (priv->serial_port_fd, TIOCMGET, &stat_read) == -1) {
            /* Ignore EINVAL, as some serial ports
               genuinely lack these lines */
            /* Thanks to Elie De Brauwer on ubuntu launchpad */

            // Apparently recently trying this on Linux PTYs fails with ENOTTY
            // instead, so exempt this as well
            if (errno != EINVAL && errno != ENOTTY) {
                GError *error = NULL;

                gt_serial_port_close (self);
                error = g_error_new (G_IO_ERROR,
                                     g_io_error_from_errno (errno),
                                     _ ("Control signals read failed: %s"),
                                     g_strerror (errno));
                gt_serial_port_set_status (
                    self, GT_SERIAL_PORT_STATE_ERROR, error);
            }

            return -2;
        }

        return stat_read;
    }
    return -1;
}

/*
 * Find out name to use for lockfile when locking tty.
 */
static char *
mbasename (char *s, char *res, int reslen)
{
    char *p;

    if (strncmp (s, "/dev/", 5) == 0) {
        /* In /dev */
        strncpy (res, s + 5, reslen - 1);
        res[reslen - 1] = 0;
        for (p = res; *p; p++)
            if (*p == '/')
                *p = '_';
    } else {
        /* Outside of /dev. Do something sensible. */
        if ((p = strrchr (s, '/')) == NULL)
            p = s;
        else
            p++;
        strncpy (res, p, reslen - 1);
        res[reslen - 1] = 0;
    }

    return res;
}

gboolean
gt_serial_port_lock (GtSerialPort *self, char *port, GError **error)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);
    char buf[128];
    char *username;
    struct stat stt;
    int fd, n = 0;
    int pid;
    int mask;
    int res;
    uid_t real_uid;
    gid_t real_gid;

    real_uid = getuid ();
    real_gid = getgid ();
    username = (getpwuid (real_uid))->pw_name;

    /* First see if the lock file directory is present. */
    if (P_LOCK[0] && stat (P_LOCK, &stt) == 0) {
        mbasename (port, buf, sizeof (buf));
        snprintf (
            priv->lockfile, sizeof (priv->lockfile), "%s/LCK..%s", P_LOCK, buf);
    } else
        priv->lockfile[0] = 0;

    if (priv->lockfile[0] && (fd = open (priv->lockfile, O_RDONLY)) >= 0) {
        n = read (fd, buf, 127);
        close (fd);
        if (n > 0) {
            pid = -1;
            if (n == 4)
                /* Kermit-style lockfile. */
                pid = *(int *)buf;
            else {
                /* Ascii lockfile. */
                buf[n] = 0;
                sscanf (buf, "%d", &pid);
            }
            if (pid > 0 && kill ((pid_t)pid, 0) < 0 && errno == ESRCH) {
                g_warning (_ ("Lockfile is stale. Overriding it…"));
                sleep (1);
                unlink (priv->lockfile);
            } else
                n = 0;
        }

        if (n == 0) {
            g_propagate_error (error,
                               g_error_new (G_IO_ERROR,
                                            G_IO_ERROR_FAILED,
                                            _ ("Device %s is locked."),
                                            port));
            g_critical (_ ("Device %s is locked."), port);
            priv->lockfile[0] = 0;

            return FALSE;
        }
    }

    if (priv->lockfile[0]) {
        /* Create lockfile compatible with UUCP-1.2 */
        mask = umask (022);
        if ((fd = open (priv->lockfile, O_WRONLY | O_CREAT | O_EXCL, 0666)) <
            0) {
            int saved_errno = errno;
            GError *inner_error = NULL;

            inner_error = g_error_new (G_IO_ERROR,
                                       g_io_error_from_errno (saved_errno),
                                       _ ("Cannot create lockfile: %s"),
                                       g_strerror (saved_errno));

            g_propagate_error (error, inner_error);
            g_critical (_ ("Cannot create lockfile. Sorry."));
            priv->lockfile[0] = 0;

            return FALSE;
        }

        (void)umask (mask);
        res = chown (priv->lockfile, real_uid, real_gid);
        if (res < 0) {
            int saved_errno = errno;
            g_warning (_ ("Lockfile chown failed: %s"),
                       g_strerror (saved_errno));
        }

        snprintf (buf,
                  sizeof (buf),
                  "%10ld sellerie %.20s\n",
                  (long)getpid (),
                  username);
        res = write (fd, buf, strlen (buf));
        if (res < 0) {
            int saved_errno = errno;
            g_warning (_ ("Lockfile write failed: %s"),
                       g_strerror (saved_errno));
        }
        close (fd);
    }

    return TRUE;
}

void
gt_serial_port_unlock (GtSerialPort *self)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    if (priv->lockfile[0])
        unlink (priv->lockfile);
}

void
gt_serial_port_close_and_unlock (GtSerialPort *self)
{
    gt_serial_port_close (self);
    gt_serial_port_unlock (self);
}

void
gt_serial_port_send_brk (GtSerialPort *self)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    if (priv->serial_port_fd == -1)
        return;
    else
        tcsendbreak (priv->serial_port_fd, 0);
}

#ifdef HAVE_LINUX_SERIAL_H
void
gt_serial_port_set_custom_speed (GtSerialPort *self, int speed)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);
    struct serial_struct ser;

    ioctl (priv->serial_port_fd, TIOCGSERIAL, &ser);
    ser.custom_divisor = ser.baud_base / speed;
    if (!(ser.custom_divisor))
        ser.custom_divisor = 1;

    ser.flags &= ~ASYNC_SPD_MASK;
    ser.flags |= ASYNC_SPD_CUST;

    ioctl (priv->serial_port_fd, TIOCSSERIAL, &ser);
}
#endif

gchar *
gt_serial_port_to_string (GtSerialPort *self)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);
    gchar *msg;
    gchar parity;

    if (priv->serial_port_fd == -1) {
        msg = g_strdup (_ ("No open port"));
    } else {
        const char *nick =
            gt_get_value_nick (GT_TYPE_SERIAL_PORT_PARITY, priv->config.parity);

        if (nick == NULL)
            parity = 'N';
        else
            parity = g_ascii_toupper (nick[0]);

        /* "Sellerie: device  baud-bits-parity-stops"  */
        msg = g_strdup_printf ("%.15s  %d-%d-%c-%d",
                               priv->config.port,
                               priv->config.vitesse,
                               priv->config.bits,
                               parity,
                               priv->config.stops);
    }

    return msg;
}

static void
gt_serial_port_set_status (GtSerialPort *self,
                           GtSerialPortState status,
                           GError *error)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    if (priv->last_error != NULL) {
        g_error_free (priv->last_error);
    }

    priv->last_error = error;

    if (priv->state != status) {
        priv->state = status;
    }

    g_object_notify (G_OBJECT (self), "status");
}

static void
gt_serial_port_class_init (GtSerialPortClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->set_property = gt_serial_port_set_property;
    object_class->get_property = gt_serial_port_get_property;
    object_class->dispose = gt_serial_port_dispose;
    object_class->finalize = gt_serial_port_finalize;

    gt_serial_port_properties[PROP_STATUS] =
        g_param_spec_enum ("status",
                           "status",
                           "status",
                           GT_TYPE_SERIAL_PORT_STATE,
                           GT_SERIAL_PORT_STATE_OFFLINE,
                           G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);

    gt_serial_port_properties[PROP_ERROR] = g_param_spec_pointer (
        "error", "error", "error", G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);

    gt_serial_port_properties[PROP_CONTROL] =
        g_param_spec_int ("control",
                          "control",
                          "control",
                          0,
                          G_MAXINT,
                          0,
                          G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);

    gt_serial_port_properties[PROP_LOCAL_ECHO] =
        g_param_spec_boolean ("local-echo",
                              "local-echo",
                              "local-echo",
                              FALSE,
                              G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);

    gt_serial_port_properties[PROP_CRLF] =
        g_param_spec_boolean ("crlf",
                              "crlf",
                              "crlf",
                              FALSE,
                              G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);

    g_object_class_install_properties (
        object_class, N_PROPERTIES, gt_serial_port_properties);

    SIGNALS[SIGNAL_DATA_AVAILABLE] = g_signal_new ("data-available",
                                                   GT_TYPE_SERIAL_PORT,
                                                   G_SIGNAL_RUN_FIRST,
                                                   0,
                                                   NULL,
                                                   NULL,
                                                   NULL,
                                                   G_TYPE_NONE,
                                                   1,
                                                   G_TYPE_BYTES,
                                                   0);
}

static void
gt_serial_port_init (GtSerialPort *self)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    priv->serial_port_fd = -1;
    priv->state = GT_SERIAL_PORT_STATE_OFFLINE;
}

static void
gt_serial_port_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GtSerialPort *self = GT_SERIAL_PORT (object);

    switch (property_id) {
    case PROP_LOCAL_ECHO:
        gt_serial_port_set_local_echo (self, g_value_get_boolean (value));
        break;
    case PROP_CRLF:
        gt_serial_port_set_crlfauto (self, g_value_get_boolean (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gt_serial_port_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    GtSerialPort *self = GT_SERIAL_PORT (object);
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    switch (property_id) {
    case PROP_STATUS:
        g_value_set_enum (value, priv->state);
        break;
    case PROP_LOCAL_ECHO:
        g_value_set_boolean (value, priv->config.echo);
        break;
    case PROP_CRLF:
        g_value_set_boolean (value, priv->config.crlfauto);
        break;
    case PROP_ERROR:
        g_value_set_pointer (value, priv->last_error);
        break;
    case PROP_CONTROL:
        g_value_set_int (value, priv->control_flags);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    };
}

static void
gt_serial_port_finalize (GObject *object)
{
    GtSerialPort *self = GT_SERIAL_PORT (object);
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);
    GObjectClass *object_class = NULL;

    g_clear_error (&priv->last_error);

    object_class = G_OBJECT_CLASS (gt_serial_port_parent_class);
    object_class->finalize (object);
}

static void
gt_serial_port_dispose (GObject *object)
{
    GtSerialPort *self = GT_SERIAL_PORT (object);
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);
    GObjectClass *object_class = NULL;

    g_cancellable_cancel (priv->cancellable);

    object_class = G_OBJECT_CLASS (gt_serial_port_parent_class);
    object_class->dispose (object);
}

static gboolean
gt_serial_port_on_control_signals_read (gpointer data)
{
    GtSerialPort *self = GT_SERIAL_PORT (data);
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);
    int control_flags = 0;

    control_flags = gt_serial_port_read_signals (self);
    if (control_flags < 0) {
        priv->status_timeout = 0;

        return FALSE;
    }

    if (control_flags != priv->control_flags) {
        priv->control_flags = control_flags;

        g_object_notify (G_OBJECT (self), "control");
    }

    return TRUE;
}

GtSerialPort *
gt_serial_port_new (void)
{
    return GT_SERIAL_PORT (g_object_new (GT_TYPE_SERIAL_PORT, NULL));
}

GError *
gt_serial_port_get_last_error (GtSerialPort *self)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    return priv->last_error;
}

GtSerialPortState
gt_serial_port_get_status (GtSerialPort *self)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    return priv->state;
}

static gboolean
on_serial_io_async_write (GObject *source, gpointer user_data)
{
    GTask *task = G_TASK (user_data);

    if (g_task_return_error_if_cancelled (task)) {
        g_object_unref (task);

        return FALSE;
    }

    GtSerialPort *self = GT_SERIAL_PORT (g_task_get_source_object (task));
    GBytes *data = g_task_get_task_data (task);
    gsize length = 0;
    gconstpointer buffer = g_bytes_get_data (data, &length);

    GError *write_error = NULL;
    gsize bytes_written =
        gt_serial_port_write (self, (const char *)buffer, length, &write_error);
    if (write_error != NULL) {
        g_task_return_error (task, write_error);
        g_object_unref (task);

        return FALSE;
    }

    if (bytes_written == 0 && length > 0) {
        g_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 _ ("Failed to write data to serial port"));
        g_object_unref (task);

        return FALSE;
    }

    // Partial write; continue
    if (bytes_written < length) {
        g_debug ("=> underwrite... setting up new slice");
        GBytes *new_data = g_bytes_new_from_bytes (
            data, bytes_written, length - bytes_written);
        g_task_set_task_data (task, new_data, (GDestroyNotify)g_bytes_unref);

        return TRUE;
    }

    g_task_return_int (task, bytes_written);
    g_object_unref (task);

    return FALSE;
}

void
gt_serial_port_write_bytes_async (GtSerialPort *self,
                                  GBytes *bytes,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);
    GTask *task = g_task_new (self, cancellable, callback, user_data);
    if (priv->last_error != NULL) {
        g_task_return_error (task, g_error_copy (priv->last_error));
        return;
    }

    g_task_set_task_data (
        task, g_bytes_ref (bytes), (GDestroyNotify)g_bytes_unref);

    GSource *source = g_pollable_output_stream_create_source (
        G_POLLABLE_OUTPUT_STREAM (priv->output_stream), cancellable);
    g_task_attach_source (task, source, (GSourceFunc)on_serial_io_async_write);
}

gsize
gt_serial_port_write_bytes_finish (GtSerialPort *self,
                                   GAsyncResult *result,
                                   GError **error)
{
    g_return_val_if_fail (g_task_is_valid (G_TASK (result), self), 0);

    return (gsize)g_task_propagate_int (G_TASK (result), error);
}

#ifdef HAVE_GUDEV
static gboolean
probe_port (const char *name)
{
    gboolean retval = FALSE;
    struct serial_struct serinfo = {0};

    int fd = open (name, O_RDWR | O_NONBLOCK | O_NOCTTY);

    if (fd < 0) {
        goto probe_port_out;
    }

    if (ioctl (fd, TIOCGSERIAL, &serinfo) == 0)
        retval = serinfo.type != PORT_UNKNOWN ? TRUE : FALSE;

probe_port_out:
    if (fd > -1)
        close (fd);

    return retval;
}
#else
static const gchar *devices_to_check[] = {"/dev/ttyS%d",
                                          "/dev/tts/%d",
                                          "/dev/ttyUSB%d",
                                          "/dev/ttyACM%d",
                                          "/dev/usb/tts/%d",
                                          NULL};

#define DEVICE_NUMBERS_TO_CHECK 12

#endif

GList *
gt_serial_port_detect_devices (void)
{
    GList *result = NULL;
#ifdef HAVE_GUDEV
    {
        GList *iter = NULL;
        const char *subsystems[] = {"tty", NULL};

        GUdevClient *client = g_udev_client_new (subsystems);
        GList *devices =
            g_udev_client_query_by_subsystem (client, subsystems[0]);

        for (iter = devices; iter != NULL; iter = iter->next) {
            GUdevDevice *p =
                g_udev_device_get_parent (G_UDEV_DEVICE (iter->data));
            if (p != NULL) {
                const char *driver = g_udev_device_get_driver (p);
                const char *device_file =
                    g_udev_device_get_device_file (G_UDEV_DEVICE (iter->data));

                if (driver != NULL) {
                    if (g_str_equal (driver, "serial8250")) {
                        if (probe_port (device_file)) {
                            result =
                                g_list_prepend (result, g_strdup (device_file));
                        }
                    } else {
                        result =
                            g_list_prepend (result, g_strdup (device_file));
                    }
                }

                g_object_unref (p);
            }
        }

        g_list_free_full (devices, g_object_unref);
        g_object_unref (client);
    }
#else
    guint i;
    const gchar **dev = NULL;
    struct stat my_stat;

    for (dev = devices_to_check; *dev != NULL; dev++) {
        for (i = 0; i < DEVICE_NUMBERS_TO_CHECK; i++) {
            gchar *device_name = NULL;

            device_name = g_strdup_printf (*dev, i);
            if (stat (device_name, &my_stat) == 0) {
                result = g_list_prepend (result, device_name);
            } else
                g_free (device_name);
        }
    }

#endif

    result = g_list_reverse (result);

    return result;
}

int
gt_get_value_by_nick (GType type, const char *value, int fallback)
{
    g_return_val_if_fail (value != NULL, fallback);

    int retval = fallback;
    GEnumClass *klass = g_type_class_ref (type);
    char *nick = g_ascii_strdown (value, -1);
    GEnumValue *ev = g_enum_get_value_by_nick (klass, nick);

    if (ev == NULL) {
        goto out;
    }

    retval = ev->value;

out:
    g_free (nick);
    g_type_class_unref (klass);

    return retval;
}

const char *
gt_get_value_nick (GType type, gint value)
{
    const char *retval = NULL;
    GEnumClass *klass = g_type_class_ref (type);
    GEnumValue *ev = g_enum_get_value (klass, value);

    if (ev == NULL) {
        goto out;
    }

    retval = ev->value_nick;

out:
    g_type_class_unref (klass);

    return retval;
}

GtSerialPortParity
gt_serial_port_parity_from_string (const char *name)
{
    return gt_get_value_by_nick (
        GT_TYPE_SERIAL_PORT_PARITY, name, GT_SERIAL_PORT_PARITY_NONE);
}

GtSerialPortFlowControl
gt_serial_port_flow_control_from_string (const char *name)
{
    return gt_get_value_by_nick (GT_TYPE_SERIAL_PORT_FLOW_CONTROL,
                                 name,
                                 GT_SERIAL_PORT_FLOW_CONTROL_NONE);
}

GtFileTransfer *
gt_serial_port_send_file (GtSerialPort *self, GFile *file)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    return g_object_new (GT_TYPE_FILE_TRANSFER,
                         "file",
                         file,
                         "serial-port",
                         self,
                         "wait-character",
                         priv->config.car,
                         "delay",
                         priv->config.delai,
                         NULL);
}

static void
gt_serial_port_on_data_ready (GObject *source,
                              GAsyncResult *res,
                              gpointer user_data)
{
    GtSerialPort *self = GT_SERIAL_PORT (user_data);
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);
    GError *error = NULL;

    GBytes *data =
        g_input_stream_read_bytes_finish (G_INPUT_STREAM (source), res, &error);

    if (error != NULL) {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            gt_serial_port_close (self);
            gt_serial_port_set_status (
                self, GT_SERIAL_PORT_STATE_OFFLINE, NULL);
        } else {
            g_clear_error (&error);
        }

        return;
    }

    g_signal_emit (self, SIGNALS[SIGNAL_DATA_AVAILABLE], 0, data);

    g_input_stream_read_bytes_async (priv->input_stream,
                                     RECEIVE_BUFFER_SIZE,
                                     G_PRIORITY_DEFAULT,
                                     priv->cancellable,
                                     gt_serial_port_on_data_ready,
                                     self);
}
