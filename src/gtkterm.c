/*
 *   This file is part of GtkTerm.
 *
 *   GtkTerm is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   GtkTerm is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with GtkTerm.  If not, see <http://www.gnu.org/licenses/>.
 */

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
extern struct configuration_port config;
GtBuffer *buffer;
extern GtkWidget *Fenetre;

static void
on_gtk_application_startup (GApplication *app,
                            gpointer      user_data)
{
  GtkBuilder *builder = gtk_builder_new_from_resource ("/org/jensge/GtkTerm/main-window.ui");
  GMenuModel *menu_model = G_MENU_MODEL (gtk_builder_get_object (builder, "window-menu"));
  g_critical ("Menu model: %p", menu_model);
  gtk_application_set_menubar (GTK_APPLICATION (app), menu_model);

  g_object_unref (builder);
}

static void
on_gtk_application_activate (GApplication *app,
                             gpointer      user_data)
{
  create_main_window(GTK_APPLICATION (app));

#if 0
  if (read_command_line(argc, argv) < 0)
  {
      exit (EXIT_FAILURE);
  }
#endif

  gt_serial_port_config (serial_port, &config);

  add_shortcuts();

  set_view(ASCII_VIEW);
}

int main(int argc, char *argv[])
{
  char *config_file;
  GtkApplication *app = NULL;
  int status;

  buffer = gt_buffer_new ();
  serial_port = gt_serial_port_new ();
  gt_serial_port_set_buffer (serial_port, buffer);
  g_object_unref (G_OBJECT (buffer));

  config_file = g_strdup_printf("%s/.gtktermrc", getenv("HOME"));
  gt_config_set_file_path (config_file);
  g_free (config_file);

  bindtextdomain(PACKAGE, LOCALEDIR);
  bind_textdomain_codeset(PACKAGE, "UTF-8");
  textdomain(PACKAGE);

  app = gtk_application_new ("org.jensge.GtkTerm",
//                             G_APPLICATION_HANDLES_COMMAND_LINE |
                             G_APPLICATION_NON_UNIQUE);

  g_signal_connect(app, "activate", G_CALLBACK (on_gtk_application_activate), NULL);
  g_signal_connect(app, "startup", G_CALLBACK (on_gtk_application_startup), NULL);

  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  gt_serial_port_close_and_unlock (serial_port);
  g_object_unref (serial_port);

  return status;
}
