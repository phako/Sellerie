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

#include "cmdline.h"
#include "fichier.h"
#include "i18n.h"
#include "sellerie-enums.h"
#include "term_config.h"
#include "util.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

extern struct configuration_port config;

static gboolean
on_parity_parse (const gchar *name,
                 const gchar *value,
                 gpointer data,
                 GError **error)
{
    int val = gt_get_value_by_nick (GT_TYPE_SERIAL_PORT_PARITY, value, -1);

    if (val == -1) {
        g_set_error (error,
                     G_OPTION_ERROR,
                     G_OPTION_ERROR_FAILED,
                     _ ("Invalid parity value (none, even, odd): %s"),
                     value);

        return FALSE;
    }

    config.parity = val;

    return TRUE;
}

static gboolean
on_config_parse (const gchar *name,
                 const gchar *value,
                 gpointer data,
                 GError **error)
{
    Load_configuration_from_file (value);

    return TRUE;
}

static gboolean
on_flow_parse (const gchar *name,
               const gchar *value,
               gpointer data,
               GError **error)
{
    int val = gt_get_value_by_nick (GT_TYPE_SERIAL_PORT_PARITY, value, -1);

    if (val == -1) {
        g_set_error (
            error,
            G_OPTION_ERROR,
            G_OPTION_ERROR_FAILED,
            _ ("Invalid flow control value (none, Xon, RTS, RS485): %s"),
            value);
        return FALSE;
    }

    config.flow = val;
    return TRUE;
}

char *default_file = NULL;
char *config_port = NULL;

static GOptionEntry entries[] = {
    {
        "config",
        'c',
        0,
        G_OPTION_ARG_CALLBACK,
        on_config_parse,
        N_ ("Load configuration FILE"),
        "FILE",
    },
    {"speed",
     's',
     0,
     G_OPTION_ARG_INT,
     &config.vitesse,
     N_ ("Serial port SPEED (default 9600)"),
     "SPEED"},
    {"parity",
     'a',
     0,
     G_OPTION_ARG_CALLBACK,
     on_parity_parse,
     N_ ("Serial port PARITY (even|odd, default none)"),
     "PARITY"},
    {"stopbits",
     't',
     0,
     G_OPTION_ARG_INT,
     &config.stops,
     N_ ("Number of STOPBITS (default 1)"),
     "STOPBITS"},
    {"bits",
     'b',
     0,
     G_OPTION_ARG_INT,
     &config.bits,
     N_ ("Number of BITS (default 8)"),
     "BITS"},
    {"file",
     'f',
     0,
     G_OPTION_ARG_FILENAME,
     &default_file,
     N_ ("Default FILE to send (default none)"),
     "FILE"},
    {"port",
     'p',
     0,
     G_OPTION_ARG_STRING,
     &config_port,
     N_ ("Serial port DEVICE (default /dev/ttyS0)"),
     "DEVICE"},
    {"flow",
     'w',
     0,
     G_OPTION_ARG_CALLBACK,
     on_flow_parse,
     N_ ("FLOW control (Xon|RTS|RS485, default none)"),
     "FLOW"},
    {"delay",
     'd',
     0,
     G_OPTION_ARG_INT,
     &config.delai,
     N_ ("End of line DELAY in ms (default none)"),
     "DELAY"},
    {"char",
     'r',
     0,
     G_OPTION_ARG_INT,
     &config.car,
     N_ ("Wait for special CHARACTER at end of line (default none)"),
     "CHARACTER"},
    {"echo",
     'e',
     0,
     G_OPTION_ARG_NONE,
     &config.echo,
     N_ ("Switch on local echo"),
     NULL},
    {"rts_time_before",
     'x',
     0,
     G_OPTION_ARG_INT,
     &config.rs485_rts_time_before_transmit,
     N_ ("For RS485, TIME in ms before transmit with RTS on"),
     "TIME"},
    {"rts_time_after",
     'y',
     0,
     G_OPTION_ARG_INT,
     &config.rs485_rts_time_after_transmit,
     N_ ("For RS485, TIME in ms after transmit with RTS on"),
     "TIME"},
    {NULL}};

void
add_option_group (GApplication *app)
{
    g_application_add_main_option_entries (app, entries);
}
