/***********************************************************************/
/* serie.c                                                             */
/* -------                                                             */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Serial port access functions                                   */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 0.99.7 : Removed auto crlf stuff - (use macros instead)      */
/*      - 0.99.5 : changed all calls to strerror() by strerror_utf8()  */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.98.6 : new sendbreak() function                            */
/*      - 0.98.1 : lockfile implementation (based on minicom)          */
/*      - 0.98 : removed IOChannel                                     */
/*                                                                     */
/***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "serial-port.h"

#include "term_config.h"
#include "widgets.h"
#include "fichier.h"
#include "buffer.h"
#include "i18n.h"

#include <termios.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_LINUX_SERIAL_H
#include <linux/serial.h>
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>

#define GT_SERIAL_PORT_CONTROL_POLL_DELAY 100 /* in ms (for control signals) */

#define P_LOCK "/var/lock/lockdev"  /* lock file location */

/* GType for GtSerialPortState enum */
#define GT_SERIAL_PORT_STATE_TYPE (gt_serial_port_state_get_type ())
static GType
gt_serial_port_state_get_type (void)
{
    static GType gt_serial_port_state_type = 0;

    if (gt_serial_port_state_type == 0)
    {
        static GEnumValue state_types[] = {
            { GT_SERIAL_PORT_STATE_ONLINE, "Serial port is online", "online" },
            { GT_SERIAL_PORT_STATE_OFFLINE, "Serial port is offline", "offline" },
            { GT_SERIAL_PORT_STATE_ERROR, "Serial port is in error state (implies offline)", "error" },
            { 0, NULL, NULL },
        };

        gt_serial_port_state_type = g_enum_register_static ("GtSerialPortState",
                                                            state_types);
    }

    return gt_serial_port_state_type;
}

typedef struct {
    struct configuration_port config;
    struct termios termios_save;
    int serial_port_fd;

    guint callback_handler_in;
    guint callback_handler_err;
    gboolean callback_activated;
    char lockfile[128];

    GtSerialPortState state;
    GError *last_error;
    int control_flags;
    guint status_timeout;
    GtBuffer *buffer;
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
    PROP_ERROR,
    PROP_CONTROL,
    N_PROPERTIES
};

static GParamSpec *gt_serial_port_properties[N_PROPERTIES] = { NULL, };

/* Local functions prototype */
static gboolean gt_serial_port_lock (GtSerialPort *, char *, GError **);
static void gt_serial_port_unlock (GtSerialPort *);
static void gt_serial_port_close (GtSerialPort *);
static gboolean gt_serial_port_on_channel_read (GIOChannel *,
                                                GIOCondition,
                                                gpointer);
static gboolean gt_serial_port_on_channel_error (GIOChannel *,
                                                 GIOCondition,
                                                 gpointer);
static void gt_serial_port_set_status (GtSerialPort *self,
                                       GtSerialPortState state,
                                       GError *error);
static gboolean
gt_serial_port_termios_from_config (GtSerialPort *self,
                                    struct termios *xtermios_p,
                                    GError **error);
static gboolean
gt_serial_port_on_control_signals_read (gpointer);

static int
gt_serial_port_read_signals (GtSerialPort *self);

/* GObject overrides */
static void gt_serial_port_set_property (GObject *, guint, const GValue *, GParamSpec *pspec);
static void gt_serial_port_get_property (GObject *, guint, GValue *, GParamSpec *pspec);
static void gt_serial_port_finalize (GObject *object);
static void gt_serial_port_dispose (GObject *object);

/* Implementations */
gboolean
gt_serial_port_on_channel_read (GIOChannel* src, GIOCondition cond, gpointer data)
{
    GtSerialPort *self = GT_SERIAL_PORT (data);
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);
    gint bytes_read;
    static gchar c[BUFFER_RECEPTION];
    gint i;

    bytes_read = BUFFER_RECEPTION;

    while (bytes_read == BUFFER_RECEPTION)
    {
        bytes_read = read (priv->serial_port_fd, c, BUFFER_RECEPTION);
        if(bytes_read > 0)
        {
            gt_buffer_put_chars (priv->buffer, c, bytes_read, priv->config.crlfauto);

            if (priv->config.car != -1 && gt_file_get_waiting_for_char () == TRUE)
            {
                i = 0;
                while (i < bytes_read)
                {
                    if (c[i] == priv->config.car)
                    {
                        gt_file_set_waiting_for_char (FALSE);
                        add_input();
                        i = bytes_read;
                    }
                    i++;
                }
            }
        }
        else if (bytes_read == -1)
        {
            if (errno != EAGAIN)
                perror (priv->config.port);
        }
    }

    return TRUE;
}

static gboolean gt_serial_port_on_channel_error (GIOChannel* src,
                                                 GIOCondition cond,
                                                 gpointer data)
{
    gt_serial_port_close (GT_SERIAL_PORT (data));
    gt_serial_port_set_status (GT_SERIAL_PORT (data),
                               GT_SERIAL_PORT_STATE_OFFLINE,
                               NULL);

    return TRUE;
}

static gboolean
gt_serial_port_termios_from_config (GtSerialPort *self,
                                    struct termios *termios_p,
                                    GError **error)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    switch (priv->config.vitesse)
    {
    case 300: termios_p->c_cflag = B300; break;
    case 600: termios_p->c_cflag = B600; break;
    case 1200: termios_p->c_cflag = B1200; break;
    case 2400: termios_p->c_cflag = B2400; break;
    case 4800: termios_p->c_cflag = B4800; break;
    case 9600: termios_p->c_cflag = B9600; break;
    case 19200: termios_p->c_cflag = B19200; break;
    case 38400: termios_p->c_cflag = B38400; break;
    case 57600: termios_p->c_cflag = B57600; break;
    case 115200:termios_p->c_cflag = B115200; break;

    default:
#ifdef HAVE_LINUX_SERIAL_H
        gt_serial_port_set_custom_speed (self, priv->config.vitesse);
        termios_p->c_cflag |= B38400;
#else
        g_propagate_error (error,
                           g_error_new_literal (G_IO_ERROR,
                                                G_IO_ERROR_FAILED,
                                                _("Arbitrary baud rates not supported"));
        return FALSE;
#endif
    }

    switch (priv->config.bits)
    {
    case 5: termios_p->c_cflag |= CS5; break;
    case 6: termios_p->c_cflag |= CS6; break;
    case 7: termios_p->c_cflag |= CS7; break;
    case 8: termios_p->c_cflag |= CS8; break;
    default: g_assert_not_reached();
    }

    switch (priv->config.parite)
    {
    case 1: termios_p->c_cflag |= PARODD | PARENB; break;
    case 2: termios_p->c_cflag |= PARENB; break;
    default: break;
    }

    if (priv->config.stops == 2)
        termios_p->c_cflag |= CSTOPB;

    termios_p->c_cflag |= CREAD;
    termios_p->c_iflag = IGNPAR | IGNBRK;

    switch(priv->config.flux)
    {
    case 1: termios_p->c_iflag |= IXON | IXOFF; break;
    case 2: termios_p->c_cflag |= CRTSCTS; break;
    default: termios_p->c_cflag |= CLOCAL; break;
    }

    termios_p->c_oflag = 0;
    termios_p->c_lflag = 0;
    termios_p->c_cc[VTIME] = 0;
    termios_p->c_cc[VMIN] = 1;

    return TRUE;
}


int gt_serial_port_send_chars (GtSerialPort *self, char *string, int length)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);
    int bytes_written = 0;

    if(priv->serial_port_fd == -1)
        return 0;

    /* Normally it never happens, but it is better not to segfault ;) */
    if(length == 0)
        return 0;

    /* RS485 half-duplex mode ? */
    if (priv->config.flux == 3)
    {
        /* set RTS (start to send) */
        gt_serial_port_set_signals (self, 1);
        if (priv->config.rs485_rts_time_before_transmit > 0)
            usleep (priv->config.rs485_rts_time_before_transmit * 1000);
    }

    bytes_written = write(priv->serial_port_fd, string, length);

    /* RS485 half-duplex mode ? */
    if (priv->config.flux == 3)
    {
        /* wait all chars are send */
        tcdrain (priv->serial_port_fd);
        if (priv->config.rs485_rts_time_after_transmit > 0)
            usleep (priv->config.rs485_rts_time_after_transmit * 1000);
        /* reset RTS (end of send, now receiving back) */
        gt_serial_port_set_signals(self, 1 );
    }

    return bytes_written;
}

gboolean gt_serial_port_config (GtSerialPort *self,
                                struct configuration_port *config)
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
    GIOChannel *channel = NULL;

    priv->serial_port_fd = open (priv->config.port,
                                 O_RDWR | O_NOCTTY | O_NDELAY);
    if(priv->serial_port_fd == -1)
    {
        error = g_error_new (G_IO_ERROR,
                             g_io_error_from_errno (errno),
                             _("Cannot open %s: %s"),
                             priv->config.port,
                             g_strerror (errno));
        gt_serial_port_set_status (self, GT_SERIAL_PORT_STATE_ERROR, error);

        return FALSE;
    }

    if (!gt_serial_port_lock (self, priv->config.port, &error))
    {
        gt_serial_port_close (self);
        gt_serial_port_set_status (self, GT_SERIAL_PORT_STATE_ERROR, error);

        return FALSE;
    }

    tcgetattr (priv->serial_port_fd, &termios_p);
    memcpy (&(priv->termios_save), &termios_p, sizeof (struct termios));

    if (!gt_serial_port_termios_from_config (self, &termios_p, &error))
    {
        gt_serial_port_close (self);
        gt_serial_port_set_status (self, GT_SERIAL_PORT_STATE_ERROR, error);
    }

    tcsetattr (priv->serial_port_fd, TCSANOW, &termios_p);
    tcflush (priv->serial_port_fd, TCOFLUSH);
    tcflush (priv->serial_port_fd, TCIFLUSH);

    channel = g_io_channel_unix_new (priv->serial_port_fd);
    priv->callback_handler_in = g_io_add_watch_full (channel,
                                                     10,
                                                     G_IO_IN,
                                                     gt_serial_port_on_channel_read,
                                                     self,
                                                     NULL);

    channel = g_io_channel_unix_new (priv->serial_port_fd);
    priv->callback_handler_err = g_io_add_watch_full (channel,
                                                      10,
                                                      G_IO_ERR,
                                                      gt_serial_port_on_channel_error,
                                                      self, NULL);

    priv->callback_activated = TRUE;

    Set_local_echo(priv->config.echo);
    gt_serial_port_set_status (self, GT_SERIAL_PORT_STATE_ONLINE, NULL);

    priv->status_timeout = g_timeout_add (GT_SERIAL_PORT_CONTROL_POLL_DELAY,
                                          gt_serial_port_on_control_signals_read,
                                          self);

    return TRUE;
}

void gt_serial_port_set_local_echo (GtSerialPort *self, gboolean echo)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    /* Double book-keeping for now */
    priv->config.echo = echo;
}

void gt_serial_port_set_crlfauto(GtSerialPort *self, gboolean crlfauto)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    /* Double book-keeping for now */
    priv->config.crlfauto = crlfauto;
}

void
gt_serial_port_close (GtSerialPort *self)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);
    if(priv->serial_port_fd != -1)
    {
        if(priv->callback_activated == TRUE)
        {
            g_source_remove(priv->callback_handler_in);
            g_source_remove(priv->callback_handler_err);
            priv->callback_activated = FALSE;
        }
        tcsetattr(priv->serial_port_fd, TCSANOW, &(priv->termios_save));
        tcflush(priv->serial_port_fd, TCOFLUSH);
        tcflush(priv->serial_port_fd, TCIFLUSH);
        close(priv->serial_port_fd);
        priv->serial_port_fd = -1;
        gt_serial_port_set_status (self, GT_SERIAL_PORT_STATE_OFFLINE, NULL);
    }

    if (priv->status_timeout != 0)
    {
        g_source_remove (priv->status_timeout);
        priv->status_timeout = 0;
    }
}

void gt_serial_port_set_signals(GtSerialPort *self, guint param)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);
    int stat_;

    if(priv->serial_port_fd == -1)
	return;

    if(ioctl(priv->serial_port_fd, TIOCMGET, &stat_) == -1)
    {
	i18n_perror(_("Control signals read"));
	return;
    }

    /* DTR */
    if(param == 0)
    {
	if(stat_ & TIOCM_DTR)
	    stat_ &= ~TIOCM_DTR;
	else
	    stat_ |= TIOCM_DTR;
	if(ioctl(priv->serial_port_fd, TIOCMSET, &stat_) == -1)
	    i18n_perror(_("DTR write"));
    }
    /* RTS */
    else if(param == 1)
    {
	if(stat_ & TIOCM_RTS)
	    stat_ &= ~TIOCM_RTS;
	else
	    stat_ |= TIOCM_RTS;
	if(ioctl(priv->serial_port_fd, TIOCMSET, &stat_) == -1)
	    i18n_perror(_("RTS write"));
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

    if (priv->config.flux == 3)
    {
        /* reset RTS (default = receive) */
        gt_serial_port_set_signals (self, 1);
    }

    if (priv->serial_port_fd != -1)
    {
        if (ioctl (priv->serial_port_fd, TIOCMGET, &stat_read) == -1)
        {
            /* Ignore EINVAL, as some serial ports
               genuinely lack these lines */
            /* Thanks to Elie De Brauwer on ubuntu launchpad */
            if (errno != EINVAL)
            {
                GError *error = NULL;

                gt_serial_port_close (self);
                error = g_error_new (G_IO_ERROR,
                                     g_io_error_from_errno (errno),
                                     _("Control signals read failed: %s"),
                                     g_strerror (errno));
                gt_serial_port_set_status (self, GT_SERIAL_PORT_STATE_ERROR, error);
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
static char *mbasename(char *s, char *res, int reslen)
{
    char *p;

    if (strncmp(s, "/dev/", 5) == 0) {
	/* In /dev */
	strncpy(res, s + 5, reslen - 1);
	res[reslen-1] = 0;
	for (p = res; *p; p++)
	    if (*p == '/') *p = '_';
    } else {
	/* Outside of /dev. Do something sensible. */
	if ((p = strrchr(s, '/')) == NULL)
	    p = s;
	else
	    p++;
	strncpy(res, p, reslen - 1);
	res[reslen-1] = 0;
    }

    return res;
}

gboolean gt_serial_port_lock (GtSerialPort *self, char *port, GError **error)
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

    real_uid = getuid();
    real_gid = getgid();
    username = (getpwuid(real_uid))->pw_name;

    /* First see if the lock file directory is present. */
    if(P_LOCK[0] && stat(P_LOCK, &stt) == 0)
    {
        mbasename (port, buf, sizeof (buf));
        snprintf(priv->lockfile, sizeof(priv->lockfile), "%s/LCK..%s", P_LOCK, buf);
    }
    else
        priv->lockfile[0] = 0;

    if(priv->lockfile[0] && (fd = open(priv->lockfile, O_RDONLY)) >= 0)
    {
        n = read(fd, buf, 127);
        close(fd);
        if(n > 0)
        {
            pid = -1;
            if(n == 4)
                /* Kermit-style lockfile. */
                pid = *(int *)buf;
            else {
                /* Ascii lockfile. */
                buf[n] = 0;
                sscanf(buf, "%d", &pid);
            }
            if(pid > 0 && kill((pid_t)pid, 0) < 0 && errno == ESRCH)
            {
                i18n_fprintf(stderr, _("Lockfile is stale. Overriding it…\n"));
                sleep(1);
                unlink(priv->lockfile);
            }
            else
                n = 0;
        }

        if(n == 0)
        {
            g_propagate_error (error,
                               g_error_new (G_IO_ERROR,
                                            G_IO_ERROR_FAILED,
                                            _("Device %s is locked."),
                                            port));
            i18n_fprintf(stderr, _("Device %s is locked.\n"), port);
            priv->lockfile[0] = 0;

            return FALSE;
        }
    }

    if(priv->lockfile[0])
    {
        /* Create lockfile compatible with UUCP-1.2 */
        mask = umask(022);
        if((fd = open(priv->lockfile, O_WRONLY | O_CREAT | O_EXCL, 0666)) < 0)
        {
            GError *inner_error = NULL;

            inner_error = g_error_new (G_IO_ERROR,
                                       g_io_error_from_errno (errno),
                                       _("Cannot create lockfile: %s"),
                                       g_strerror (errno));

            g_propagate_error (error, inner_error);
            i18n_fprintf(stderr, _("Cannot create lockfile. Sorry.\n"));
            priv->lockfile[0] = 0;

            return FALSE;
        }
        (void)umask(mask);
        res = chown(priv->lockfile, real_uid, real_gid);
        if (res < 0)
            i18n_fprintf(stderr, "Fail");

        snprintf(buf, sizeof(buf), "%10ld gtkterm %.20s\n", (long)getpid(), username);
        res = write(fd, buf, strlen(buf));
        if (res < 0)
            i18n_fprintf(stderr, "Fail");
        close(fd);
    }

    return TRUE;
}

void gt_serial_port_unlock (GtSerialPort *self)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    if(priv->lockfile[0])
        unlink(priv->lockfile);
}

void gt_serial_port_close_and_unlock (GtSerialPort *self)
{
    gt_serial_port_close (self);
    gt_serial_port_unlock (self);
}

void gt_serial_port_send_brk (GtSerialPort *self)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    if (priv->serial_port_fd == -1)
        return;
    else
        tcsendbreak(priv->serial_port_fd, 0);
}

#ifdef HAVE_LINUX_SERIAL_H
void gt_serial_port_set_custom_speed(GtSerialPort *self, int speed)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);
    struct serial_struct ser;

    ioctl(priv->serial_port_fd, TIOCGSERIAL, &ser);
    ser.custom_divisor = ser.baud_base / speed;
    if(!(ser.custom_divisor))
	ser.custom_divisor = 1;

    ser.flags &= ~ASYNC_SPD_MASK;
    ser.flags |= ASYNC_SPD_CUST;

    ioctl(priv->serial_port_fd, TIOCSSERIAL, &ser);
}
#endif

gchar* gt_serial_port_to_string (GtSerialPort *self)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);
    gchar* msg;
    gchar parity;

    if(priv->serial_port_fd == -1)
    {
        msg = g_strdup(_("No open port"));
    } else {
        // 0: none, 1: odd, 2: even
        switch (priv->config.parite)
        {
        case 0:
            parity = 'N';
            break;
        case 1:
            parity = 'O';
            break;
        case 2:
            parity = 'E';
            break;
        default:
            parity = 'N';
        }

        /* "GtkTerm: device  baud-bits-parity-stops"  */
        msg = g_strdup_printf("%.15s  %d-%d-%c-%d",
                              priv->config.port,
                              priv->config.vitesse,
                              priv->config.bits,
                              parity,
                              priv->config.stops);
    }

    return msg;
}

int gt_serial_port_get_fd (GtSerialPort *self)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    return priv->serial_port_fd;
}

static void gt_serial_port_set_status (GtSerialPort *self,
                                       GtSerialPortState status,
                                       GError *error)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    if (priv->last_error != NULL)
    {
        g_error_free (priv->last_error);
    }

    priv->last_error = error;

    if (priv->state != status)
    {
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
        g_param_spec_enum ("status", "status", "status",
                           GT_SERIAL_PORT_STATE_TYPE,
                           GT_SERIAL_PORT_STATE_OFFLINE,
                           G_PARAM_STATIC_STRINGS |
                           G_PARAM_READABLE);

    gt_serial_port_properties[PROP_ERROR] =
        g_param_spec_pointer ("error", "error", "error",
                              G_PARAM_STATIC_STRINGS |
                              G_PARAM_READABLE);

    gt_serial_port_properties[PROP_CONTROL] =
        g_param_spec_int ("control", "control", "control",
                          0,
                          G_MAXINT,
                          0,
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_READABLE);

    g_object_class_install_properties (object_class,
                                       N_PROPERTIES,
                                       gt_serial_port_properties);
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
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
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

    g_clear_error (&priv->last_error);
}

static void
gt_serial_port_dispose (GObject *object)
{
    GtSerialPort *self = GT_SERIAL_PORT (object);
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    g_clear_object (&priv->buffer);
}

static gboolean
gt_serial_port_on_control_signals_read (gpointer data)
{
    GtSerialPort *self = GT_SERIAL_PORT (data);
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);
    int control_flags = 0;

    control_flags = gt_serial_port_read_signals (self);
    if (control_flags < 0)
    {
        priv->status_timeout = 0;

        return FALSE;
    }

    if (control_flags != priv->control_flags)
    {
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

void
gt_serial_port_set_buffer (GtSerialPort *self, GtBuffer *buffer)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    priv->buffer = g_object_ref (buffer);
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

GtBuffer *
gt_serial_port_get_buffer (GtSerialPort *self)
{
    GtSerialPortPrivate *priv = gt_serial_port_get_instance_private (self);

    return priv->buffer;
}
