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
#include "buffer.h"
#include "macros.h"

#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>

GtSerialPort *serial_port;
GtBuffer *buffer;

int main(int argc, char *argv[])
{
    GSettings *settings = NULL;

    bindtextdomain(PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(PACKAGE, "UTF-8");
    textdomain(PACKAGE);

    gtk_init(&argc, &argv);

    settings = gt_config_get_profile_settings ();

    buffer = gt_buffer_new ();
    serial_port = gt_serial_port_new (settings);
    gt_serial_port_set_buffer (serial_port, buffer);
    g_object_unref (G_OBJECT (buffer));

    create_main_window(settings);

    /*
       if (read_command_line(argc, argv) < 0)
       {
       exit (EXIT_FAILURE);
       }
       */

    add_shortcuts();

    set_view(ASCII_VIEW);

    gtk_main();

    gt_serial_port_close_and_unlock (serial_port);
    g_object_unref (serial_port);

    return EXIT_SUCCESS;
}
