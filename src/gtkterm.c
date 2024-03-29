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
#include "logging.h"
#include "main-window.h"
#include "parsecfg.h"
#include "serial-port.h"
#include "term_config.h"
#include "macro-manager.h"

#include <stdlib.h>

#include <gdk/gdk.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

extern struct configuration_port config;

extern char *default_file;
extern char *config_port;
GtSerialPort *serial_port;
GtkWidget *Fenetre;
GtkWidget *display;

static int
on_gtk_application_local_options (GApplication *app,
                                  GVariantDict *options,
                                  gpointer user_data)
{
    if (config_port != NULL) {
        strncpy (config.port, config_port, sizeof (config.port) - 1);
    }
    Verify_configuration ();

    return -1;
}

static void
on_gtk_application_startup (GApplication *app, gpointer user_data)
{
    GtkBuilder *builder =
        gtk_builder_new_from_resource ("/org/jensge/Sellerie/main-menu.ui");
    GMenuModel *menu_model =
        G_MENU_MODEL (gtk_builder_get_object (builder, "menubar"));

    gtk_application_set_menubar (GTK_APPLICATION (app), menu_model);

    g_object_unref (builder);
}

static void
on_gtk_application_activate (GApplication *app, gpointer user_data)
{
    GtkWidget *main_window = gt_main_window_new (GTK_APPLICATION (app));

    Fenetre = main_window;

    gtk_application_add_window (GTK_APPLICATION (app),
                                GTK_WINDOW (main_window));


    serial_port = GT_MAIN_WINDOW (main_window)->serial_port;
    display = GT_MAIN_WINDOW (main_window)->display;
    GT_MAIN_WINDOW (main_window)->default_raw_file = default_file;

    update_vte_config ();

    gt_serial_port_config (GT_MAIN_WINDOW (main_window)->serial_port, &config);

    gtk_window_present (GTK_WINDOW (main_window));
    gtk_widget_show (main_window);
}

int
main (int argc, char *argv[])
{
    char *config_file;
    GtkApplication *app = NULL;
    int status;

    config_file = g_strdup_printf ("%s/.gtktermrc", getenv ("HOME"));
    gt_config_set_file_path (config_file);
    g_free (config_file);

    bindtextdomain (PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (PACKAGE, "UTF-8");
    textdomain (PACKAGE);

    gtk_init ();

    app = gtk_application_new ("org.jensge.Sellerie", G_APPLICATION_NON_UNIQUE);
    g_object_set (G_OBJECT (gt_macro_manager_get_default ()), "app", app, NULL);
    add_option_group (G_APPLICATION (app));

    Check_configuration_file ();

    g_signal_connect (
        app, "activate", G_CALLBACK (on_gtk_application_activate), NULL);
    g_signal_connect (
        app, "startup", G_CALLBACK (on_gtk_application_startup), NULL);
    g_signal_connect (app,
                      "handle-local-options",
                      G_CALLBACK (on_gtk_application_local_options),
                      NULL);

    status = g_application_run (G_APPLICATION (app), argc, argv);
    g_object_unref (app);

    return status;
}
