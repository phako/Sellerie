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

#include "buffer.h"
#include "fichier.h"
#include "infobar.h"
#include "main-window.h"
#include "serial-port.h"
#include "term_config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>


/* Global variables */
static gint nb_car;
static gint car_written;
static gint current_buffer_position;
static gint bytes_read;
static gint Fichier;
static guint callback_handler;
static gchar *fic_defaut = NULL;
static gboolean waiting_for_char = FALSE;
static gboolean waiting_for_timer = FALSE;
static gboolean input_running = FALSE;
static gchar *str = NULL;

/* Local functions prototype */
static void
close_all (gpointer data);
static void
on_infobar_close (GtkInfoBar *bar, gpointer user_data);
static void
on_infobar_response (GtkInfoBar *bar, gint response_id, gpointer user_data);
static gboolean
on_serial_io_ready (GIOChannel *source, GIOCondition condition, gpointer data);
static gboolean
timer (gpointer pointer);
static void
remove_input (void);

extern struct configuration_port config;

void
send_ascii_file (GFile *file, GtkWindow *parent)
{
    gchar *fileName = g_file_get_path (file);
    gchar *msg;

    if (!g_file_test (fileName, G_FILE_TEST_IS_REGULAR)) {
        msg = g_strdup_printf (_ ("Error opening file\n"));
        gt_main_window_show_message (
            GT_MAIN_WINDOW (parent), msg, GT_MESSAGE_TYPE_ERROR);
        g_free (msg);
        g_free (fileName);
        return;
    }

    Fichier = open (fileName, O_RDONLY);
    if (Fichier != -1) {
        gt_file_set_default (fileName);
        msg = g_strdup_printf (_ ("%s : transfer in progress…"), fileName);
        gt_main_window_push_status (GT_MAIN_WINDOW (parent), msg);

        car_written = 0;
        current_buffer_position = 0;
        bytes_read = 0;
        nb_car = lseek (Fichier, 0L, SEEK_END);
        lseek (Fichier, 0L, SEEK_SET);

        GtkWidget *infobar = gt_infobar_new ();
        gt_main_window_set_info_bar (GT_MAIN_WINDOW (parent), infobar);
        g_signal_connect (
            G_OBJECT (infobar), "close", G_CALLBACK (on_infobar_close), parent);
        g_signal_connect (G_OBJECT (infobar),
                          "response",
                          G_CALLBACK (on_infobar_response),
                          parent);
        gt_infobar_set_label (GT_INFOBAR (infobar), msg);
        g_free (msg);

        gtk_widget_show_all (infobar);

        add_input (parent);
    } else {
        msg = g_strdup_printf (
            _ ("Cannot read file %s: %s\n"), fileName, strerror (errno));
        gt_main_window_show_message (
            GT_MAIN_WINDOW (parent), msg, GT_MESSAGE_TYPE_ERROR);
        g_free (msg);
    }
    g_free (fileName);
}

static gboolean
on_serial_io_ready (GIOChannel *source, GIOCondition condition, gpointer data)
{
    static gchar buffer[BUFFER_EMISSION];
    static gchar *current_buffer;
    static gint bytes_to_write;
    gint bytes_written;
    gchar *car;

    if (condition == G_IO_ERR) {
        str = g_strdup_printf (_ ("Error sending file\n"));
        gt_main_window_show_message (
            GT_MAIN_WINDOW (data), str, GT_MESSAGE_TYPE_ERROR);
        close_all (data);
        return FALSE;
    }

    GtkWidget *infobar = gt_main_window_get_info_bar (GT_MAIN_WINDOW (data));

    gt_infobar_set_progress (GT_INFOBAR (infobar),
                             (gdouble)car_written / (gdouble)nb_car);

    if (car_written < nb_car) {
        /* Read the file only if buffer totally sent or if buffer empty */
        if (current_buffer_position == bytes_read) {
            bytes_read = read (Fichier, buffer, BUFFER_EMISSION);

            current_buffer_position = 0;
            current_buffer = buffer;
            bytes_to_write = bytes_read;
        }

        if (current_buffer == NULL) {
            /* something went wrong... */
            g_free (str);
            str = g_strdup_printf (_ ("Error sending file\n"));
            gt_main_window_show_message (
                GT_MAIN_WINDOW (data), str, GT_MESSAGE_TYPE_ERROR);
            close_all (data);
            return FALSE;
        }

        car = current_buffer;

        if (config.delai != 0 || config.car != -1) {
            /* search for next LF */
            bytes_to_write = current_buffer_position;
            while (*car != LINE_FEED && bytes_to_write < bytes_read) {
                car++;
                bytes_to_write++;
            }
            if (*car == LINE_FEED)
                bytes_to_write++;
        }

        /* write to serial port */
        bytes_written = gt_serial_port_send_chars (
            GT_MAIN_WINDOW (data)->serial_port,
            current_buffer,
            bytes_to_write - current_buffer_position);

        if (bytes_written == -1) {
            /* Problem while writing, stop file transfer */
            g_free (str);
            str = g_strdup_printf (_ ("Error sending file: %s\n"),
                                   strerror (errno));
            gt_main_window_show_message (
                GT_MAIN_WINDOW (data), str, GT_MESSAGE_TYPE_ERROR);
            close_all (data);
            return FALSE;
        }

        car_written += bytes_written;
        current_buffer_position += bytes_written;
        current_buffer += bytes_written;

        gt_infobar_set_progress (GT_INFOBAR (infobar),
                                 (gdouble)car_written / (gdouble)nb_car);

        if (config.delai != 0 && *car == LINE_FEED) {
            remove_input ();
            g_timeout_add (config.delai, (GSourceFunc)timer, NULL);
            waiting_for_timer = TRUE;
        } else if (config.car != -1 && *car == LINE_FEED) {
            remove_input ();
            waiting_for_char = TRUE;
        }
    } else {
        close_all (data);
        return FALSE;
    }
    return TRUE;
}

static gboolean
timer (gpointer pointer)
{
    if (waiting_for_timer == TRUE) {
        add_input (pointer);
        waiting_for_timer = FALSE;
    }
    return FALSE;
}

void
add_input (gpointer user_data)
{
    if (input_running == FALSE) {
        int fd = -1;

        input_running = TRUE;
        fd = gt_serial_port_get_fd (GT_MAIN_WINDOW (user_data)->serial_port);
        callback_handler = g_io_add_watch_full (g_io_channel_unix_new (fd),
                                                10,
                                                G_IO_OUT | G_IO_ERR,
                                                (GIOFunc)on_serial_io_ready,
                                                user_data,
                                                NULL);
    }
}

static void
remove_input (void)
{
    if (input_running == TRUE) {
        g_source_remove (callback_handler);
        input_running = FALSE;
    }
}

static void
on_infobar_close (GtkInfoBar *bar, gpointer user_data)
{
    close_all (user_data);
}

static void
close_all (gpointer data)
{
    remove_input ();
    waiting_for_char = FALSE;
    waiting_for_timer = FALSE;
    GtkWidget *infobar = gt_main_window_get_info_bar (GT_MAIN_WINDOW (data));
    gt_main_window_remove_info_bar (GT_MAIN_WINDOW (data), infobar);
    gt_main_window_pop_status (GT_MAIN_WINDOW (data));
    close (Fichier);
    gtk_widget_destroy (infobar);
}

static void
on_infobar_response (GtkInfoBar *bar, gint response_id, gpointer user_data)
{
    close_all (user_data);
}

gboolean
gt_file_get_waiting_for_char (void)
{
    return waiting_for_char;
}

void
gt_file_set_waiting_for_char (gboolean waiting)
{
    waiting_for_char = waiting;
}

const char *
gt_file_get_default (void)
{
    return fic_defaut;
}

void
gt_file_set_default (const char *file)
{
    g_clear_pointer (&fic_defaut, g_free);

    if (file == NULL) {
        return;
    }

    fic_defaut = g_strdup (file);
}
