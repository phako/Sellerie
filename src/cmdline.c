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

extern struct configuration_port config;

static gboolean on_parity_parse (const gchar *name,
                                 const gchar *value,
                                 gpointer data,
                                 GError **error)
{
  if (g_str_equal (value, "odd")) {
    config.parite = 1;

    return TRUE;
  }

  if (g_str_equal (value, "even")) {
    config.parite = 2;

    return TRUE;
  }

  g_set_error (error,
               G_OPTION_ERROR,
               G_OPTION_ERROR_FAILED,
               "Invalid parity value (even, odd): %s",
               value);

  return FALSE;
}

static gboolean on_config_parse (const gchar *name,
                                 const gchar *value,
                                 gpointer data,
                                 GError **error)
{
  Load_configuration_from_file (value);

  return TRUE;
}

static gboolean on_flow_parse (const gchar *name,
                               const gchar *value,
                               gpointer data,
                               GError **error)
{
  if (g_str_equal (value, "RTS")) {
    config.flux = 1;

    return TRUE;
  }

  if (g_str_equal (value, "Xon")) {
    config.flux = 2;

    return TRUE;
  }

  if (g_str_equal (value, "RS485")) {
    config.flux = 3;

    return TRUE;
  }

  g_set_error (error,
               G_OPTION_ERROR,
               G_OPTION_ERROR_FAILED,
               "Invalid flow value (Xon, RTS, RS485): %s",
               value);

  return FALSE;
}

static char *default_file = NULL;
static char *config_port = NULL;

static GOptionEntry entries[] = {
    { "config", 'c', 0, G_OPTION_ARG_CALLBACK, on_config_parse, N_("Load configuration FILE"), "FILE", },
    { "speed", 's', 0, G_OPTION_ARG_INT, &config.vitesse, N_("Serial port SPEED (default 9600)"), "SPEED"},
    { "parity", 'a', 0, G_OPTION_ARG_CALLBACK, on_parity_parse, N_("Serial port PARITY (even|odd, default none)"), "PARITY" },
    { "stopbits", 't', 0, G_OPTION_ARG_INT, &config.stops, N_("Number of STOPBITS (default 1)"), "STOPBITS" },
    { "bits", 'b', 0, G_OPTION_ARG_INT, &config.bits, N_("Number of BITS (default 8)"), "BITS" },
    { "file", 'f', 0, G_OPTION_ARG_FILENAME, &default_file, N_("Default FILE to send (default none)"), "FILE" },
    { "port", 'p', 0, G_OPTION_ARG_STRING, &config_port, N_("Serial port DEVICE (default /dev/ttyS0)"), "DEVICE" },
    { "flow", 'w', 0, G_OPTION_ARG_CALLBACK, on_flow_parse, N_("FLOW control (Xon|RTS|RS485, default none)"), "FLOW" },
    { "delay", 'd', 0, G_OPTION_ARG_INT, &config.delai, N_("End of line DELAY in ms (default none)"), "DELAY" },
    { "char", 'r', 0, G_OPTION_ARG_INT, &config.car, N_("Wait for special CHARACTER at end of line (default none)"), "CHARACTER" },
    { "echo", 'e', 0, G_OPTION_ARG_NONE, &config.echo, N_("Switch on local echo"), NULL },
    { "rts_time_before", 'x', 0, G_OPTION_ARG_INT, &config.rs485_rts_time_before_transmit, N_("For RS485, TIME in ms before transmit with RTS on"), "TIME" },
    { "rts_time_after", 'y', 0, G_OPTION_ARG_INT, &config.rs485_rts_time_after_transmit, N_("For RS485, TIME in ms after transmit with RTS on"), "TIME" },
    { NULL }
};

int read_command_line (int argc, char **argv)
{
  GError *error = NULL;
  GOptionContext *context = NULL;
  int result = -1;

  Check_configuration_file();

  context = g_option_context_new ("- GTK+ serial console");
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_warning ("Failed to parse commandline options: %s", error->message);
  } else {
    if (default_file != NULL) {
      gt_file_set_default (default_file);
    }

    if (config_port != NULL)
    {
        strncpy (config.port, config_port, sizeof (config.port));
    }
    Verify_configuration();
    result = 0;
  }

  g_option_context_free (context);

  return result;
}

