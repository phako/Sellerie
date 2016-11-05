/***********************************************************************/
/* cmdline.c                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Reads the command line                                         */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.98.3 : modified for configuration file                     */
/*      - 0.98.2 : added --echo                                        */
/*      - 0.98 : file creation by Julien                               */
/*                                                                     */
/***********************************************************************/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "term_config.h"
#include "fichier.h"
#include "i18n.h"
#include "cmdline.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib.h>

static gboolean on_parity_parse (const gchar *name,
                                 const gchar *value,
                                 gpointer data,
                                 GError **error)
{
    GSettings *settings = G_SETTINGS (data);

    if (g_str_equal (value, "odd")) {
        g_settings_set_enum (settings, "parity", GT_SERIAL_PARITY_ODD);

        return TRUE;
    }

    if (g_str_equal (value, "even")) {
        g_settings_set_enum (settings, "parity", GT_SERIAL_PARITY_ODD);

        return TRUE;
    }

    g_set_error (error,
            G_OPTION_ERROR,
            G_OPTION_ERROR_FAILED,
            _("Invalid parity value (even, odd): %s"),
            value);

    return FALSE;
}

static gboolean on_flow_parse (const gchar *name,
                               const gchar *value,
                               gpointer data,
                               GError **error)
{
    GSettings *settings = G_SETTINGS (data);

    if (g_str_equal (value, "RTS")) {
        g_settings_set_enum (settings, "flow", GT_SERIAL_FLOW_RTS);

        return TRUE;
    }

    if (g_str_equal (value, "Xon")) {
        g_settings_set_enum (settings, "flow", GT_SERIAL_FLOW_XON);

        return TRUE;
    }

    if (g_str_equal (value, "RS485")) {
        g_settings_set_enum (settings, "flow", GT_SERIAL_FLOW_RS485);

        return TRUE;
    }

    g_set_error (error,
            G_OPTION_ERROR,
            G_OPTION_ERROR_FAILED,
            _("Invalid flow value (Xon, RTS, RS485): %s"),
            value);

    return FALSE;
}

static char *default_file = NULL;
static char *config_port = NULL;
static int config_speed = -1;
static int config_stops = -1;
static int config_bits = -1;
static int config_delay = -1;
static int config_waitchar = -1;
static gboolean config_echo = FALSE;
static int config_rs485_rts_b4tx = -1;
static int config_rs485_rts_tx = -1;

static GOptionEntry entries[] = {
    { "speed", 's', 0, G_OPTION_ARG_INT, &config_speed, N_("Serial port SPEED (default 9600)"), "SPEED"},
    { "parity", 'a', 0, G_OPTION_ARG_CALLBACK, on_parity_parse, N_("Serial port PARITY (even|odd, default none)"), "PARITY" },
    { "stopbits", 't', 0, G_OPTION_ARG_INT, &config_stops, N_("Number of STOPBITS (default 1)"), "STOPBITS" },
    { "bits", 'b', 0, G_OPTION_ARG_INT, &config_bits, N_("Number of BITS (default 8)"), "BITS" },
    { "file", 'f', 0, G_OPTION_ARG_FILENAME, &default_file, N_("Default FILE to send (default none)"), "FILE" },
    { "port", 'p', 0, G_OPTION_ARG_STRING, &config_port, N_("Serial port DEVICE (default /dev/ttyS0)"), "DEVICE" },
    { "flow", 'w', 0, G_OPTION_ARG_CALLBACK, on_flow_parse, N_("FLOW control (Xon|RTS|RS485, default none)"), "FLOW" },
    { "delay", 'd', 0, G_OPTION_ARG_INT, &config_delay, N_("End of line DELAY in ms (default none)"), "DELAY" },
    { "char", 'r', 0, G_OPTION_ARG_INT, &config_waitchar, N_("Wait for special CHARACTER at end of line (default none)"), "CHARACTER" },
    { "echo", 'e', 0, G_OPTION_ARG_NONE, &config_echo, N_("Switch on local echo"), NULL },
    { "rts_time_before", 'x', 0, G_OPTION_ARG_INT, &config_rs485_rts_b4tx, N_("For RS485, TIME in ms before transmit with RTS on"), "TIME" },
    { "rts_time_after", 'y', 0, G_OPTION_ARG_INT, &config_rs485_rts_tx, N_("For RS485, TIME in ms after transmit with RTS on"), "TIME" },
    { NULL }
};

int read_command_line (int argc, char **argv)
{
  GError *error = NULL;
  GOptionContext *context = NULL;
  GOptionGroup *group = NULL;
  int result = -1;
  GSettings *settings  = gt_config_get_profile_settings ();

  group = g_option_group_new ("default", "gtkterm", "gtkterm", settings, g_object_unref);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, entries);

  context = g_option_context_new ("- GTK+ serial console");
  g_option_context_set_main_group (context, group);
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_warning ("Failed to parse commandline options: %s", error->message);
  } else {
    if (config_port != NULL)
    {
        g_settings_set_string (settings, "port", config_port);
    }
    result = 0;
  }

  g_option_context_free (context);
  g_option_group_unref (group);
  g_free (config_port);

  return result;
}

