/***********************************************************************/
/* gtkterm.c                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Main program file                                              */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.99.0 : added call to add_shortcuts()                       */
/*      - 0.98 : all GUI functions moved to widgets.c                  */
/*                                                                     */
/***********************************************************************/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "widgets.h"
#include "serial-port.h"
#include "term_config.h"
#include "cmdline.h"
#include "parsecfg.h"
#include "buffer.h"
#include "macros.h"

#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>

GtSerialPort *serial_port;

int main(int argc, char *argv[])
{
  gchar *message;
  char *config_file;

  serial_port = gt_serial_port_new ();

  config_file = g_strdup_printf("%s/.gtktermrc", getenv("HOME"));
  gt_config_set_file_path (config_file);

  bindtextdomain(PACKAGE, LOCALEDIR);
  bind_textdomain_codeset(PACKAGE, "UTF-8");
  textdomain(PACKAGE);

  gtk_init(&argc, &argv);

  create_buffer();

  create_main_window();

  if(read_command_line(argc, argv) < 0)
    {
      delete_buffer();
      exit(1);
    }

  gt_serial_port_config (serial_port);

  message = gt_serial_port_to_string (serial_port);
  Set_window_title(message);
  gt_main_window_set_status (message);
  g_free(message);

  add_shortcuts();

  set_view(ASCII_VIEW);

  gtk_main();

  delete_buffer();

  gt_serial_port_close_and_unlock (serial_port);
  g_object_unref (serial_port);

  return 0;
}
