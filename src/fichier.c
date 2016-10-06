/***********************************************************************/
/* fichier.c                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Raw / text file transfer management                            */
/*                                                                     */
/*   ChangeLog                                                         */
/*   (All changes by Julien Schmitt except when explicitly written)    */
/*                                                                     */
/*      - 0.99.5 : changed all calls to strerror() by strerror_utf8()  */
/*      - 0.99.4 : added auto CR LF function by Sebastien              */
/*                 modified ecriture() to use send_serial()            */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.98.4 : modified to use new buffer                          */
/*      - 0.98 : file transfer completely rewritten / optimized        */
/*                                                                     */
/***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "term_config.h"
#include "widgets.h"
#include "serial-port.h"
#include "buffer.h"
#include "fichier.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

extern GtSerialPort *serial_port;

/* Global variables */
static gint nb_car;
static gint car_written;
static gint current_buffer_position;
static gint bytes_read;
static GtkWidget *ProgressBar;
static gint Fichier;
static guint callback_handler;
static gchar *fic_defaut = NULL;
static GtkWidget *Window;
static gboolean waiting_for_char = FALSE;
static gboolean waiting_for_timer = FALSE;
static gboolean input_running = FALSE;
static gchar *str = NULL;
static FILE *Fic;

/* Local functions prototype */
static gint close_all(void);
static gboolean on_serial_io_ready (GIOChannel *source, GIOCondition condition, gpointer data);
static gboolean timer(gpointer pointer);
static void remove_input(void);
static void write_file(char *, unsigned int);

extern struct configuration_port config;

void send_raw_file(GtkWindow *parent)
{
	GtkWidget *file_select;

	file_select = gtk_file_chooser_dialog_new(_("Send RAW File"),
	                                          parent,
	                                          GTK_FILE_CHOOSER_ACTION_OPEN, 
	                                          _("_Cancel"), GTK_RESPONSE_CANCEL,
	                                          _("_OK"), GTK_RESPONSE_ACCEPT,
	                                          NULL);

	if(fic_defaut != NULL)
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(file_select), fic_defaut);

	if(gtk_dialog_run(GTK_DIALOG(file_select)) == GTK_RESPONSE_ACCEPT)
	{
		gchar *fileName;
		gchar *msg;

		fileName = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_select));

		if(!g_file_test(fileName, G_FILE_TEST_IS_REGULAR))
		{
			msg = g_strdup_printf(_("Error opening file\n"));
			show_message(msg, MSG_ERR);
			g_free(msg);
			g_free(fileName);
			gtk_widget_destroy(file_select);
			return;
		}

		Fichier = open(fileName, O_RDONLY);
		if(Fichier != -1)
		{
			GtkWidget *Bouton_annuler, *Box;

			fic_defaut = g_strdup(fileName);
			msg = g_strdup_printf(_("%s : transfer in progress…"), fileName);
            gt_main_window_push_status (msg);

			car_written = 0;
			current_buffer_position = 0;
			bytes_read = 0;
			nb_car = lseek(Fichier, 0L, SEEK_END);
			lseek(Fichier, 0L, SEEK_SET);

			Window = gtk_dialog_new();
			gtk_window_set_title(GTK_WINDOW(Window), msg);
			g_free(msg);
			Box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
			gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(Window))), Box);
			ProgressBar = gtk_progress_bar_new();

			gtk_box_pack_start(GTK_BOX(Box), ProgressBar, FALSE, FALSE, 5);

			Bouton_annuler = gtk_button_new_with_label(_("_Cancel"));
			g_signal_connect(GTK_WIDGET(Bouton_annuler), "clicked", G_CALLBACK(close_all), NULL);
            gtk_dialog_add_action_widget (GTK_DIALOG (Window), Bouton_annuler, GTK_RESPONSE_CANCEL);

			g_signal_connect(GTK_WIDGET(Window), "delete_event", G_CALLBACK(close_all), NULL);

			gtk_window_set_default_size(GTK_WINDOW(Window), 250, 100);
			gtk_window_set_modal(GTK_WINDOW(Window), TRUE);
			gtk_widget_show_all(Window);

			add_input();
		}
		else
		{
			msg = g_strdup_printf(_("Cannot read file %s: %s\n"), fileName, strerror(errno));
			show_message(msg, MSG_ERR);
			g_free(msg);
		}
		g_free(fileName);
	}
	gtk_widget_destroy(file_select);
}

static gboolean on_serial_io_ready (GIOChannel *source,
                                GIOCondition condition,
                                gpointer data)
{
    static gchar buffer[BUFFER_EMISSION];
    static gchar *current_buffer;
    static gint bytes_to_write;
    gint bytes_written;
    gchar *car;

    if (condition == G_IO_ERR) {
        str = g_strdup_printf (_("Error sending file\n"));
        show_message(str, MSG_ERR);
        close_all();
        return FALSE;
    }

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar), (gfloat)car_written/(gfloat)nb_car );

    if(car_written < nb_car)
    {
	/* Read the file only if buffer totally sent or if buffer empty */
	if(current_buffer_position == bytes_read)
	{
	    bytes_read = read(Fichier, buffer, BUFFER_EMISSION);

	    current_buffer_position = 0;
	    current_buffer = buffer;
	    bytes_to_write = bytes_read;
	}

    if(current_buffer == NULL)
    {
        /* something went wrong... */
        g_free(str);
        str = g_strdup_printf(_("Error sending file\n"));
        show_message(str, MSG_ERR);
        close_all();
        return FALSE;
    }

	car = current_buffer;

	if(config.delai != 0 || config.car != -1)
	{
	    /* search for next LF */
	    bytes_to_write = current_buffer_position;
	    while(*car != LINE_FEED && bytes_to_write < bytes_read)
	    {
		car++;
		bytes_to_write++;
	    }
	    if(*car == LINE_FEED)
		bytes_to_write++;
	}

	/* write to serial port */
	bytes_written = send_serial(current_buffer, bytes_to_write - current_buffer_position);

	if(bytes_written == -1)
	{
	    /* Problem while writing, stop file transfer */
	    g_free(str);
	    str = g_strdup_printf(_("Error sending file: %s\n"), strerror(errno));
	    show_message(str, MSG_ERR);
	    close_all();
	    return FALSE;
	}

	car_written += bytes_written;
	current_buffer_position += bytes_written;
	current_buffer += bytes_written;

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar), (gfloat)car_written/(gfloat)nb_car );

	if(config.delai != 0 && *car == LINE_FEED)
	{
	    remove_input();
	    g_timeout_add(config.delai, (GSourceFunc)timer, NULL);
	    waiting_for_timer = TRUE;
	}
	else if(config.car != -1 && *car == LINE_FEED)
	{
	    remove_input();
	    waiting_for_char = TRUE;
	}
    }
    else
    {
	close_all();
	return FALSE;
    }
    return TRUE;
}

static gboolean timer(gpointer pointer)
{
    if(waiting_for_timer == TRUE)
    {
	add_input();
	waiting_for_timer = FALSE;
    }
    return FALSE;
}

void add_input(void)
{
    if(input_running == FALSE)
    {
        int fd = -1;

        input_running = TRUE;
        fd = gt_serial_port_get_fd (serial_port);
        callback_handler = g_io_add_watch_full(g_io_channel_unix_new(fd),
                                               10,
                                               G_IO_OUT | G_IO_ERR,
                                               (GIOFunc)on_serial_io_ready,
                                               NULL, NULL);

    }
}

static void remove_input(void)
{
    if(input_running == TRUE)
    {
    g_source_remove(callback_handler);
	input_running = FALSE;
    }
}

static gint close_all(void)
{
    remove_input();
    waiting_for_char = FALSE;
    waiting_for_timer = FALSE;
    gt_main_window_pop_status ();
    close(Fichier);
    gtk_widget_destroy(Window);

    return FALSE;
}

static void write_file(char *data, unsigned int size)
{
    fwrite(data, size, 1, Fic);
}

void save_raw_file(GtkWindow *parent)
{
	GtkWidget *file_select;

	file_select = gtk_file_chooser_dialog_new(_("Save RAW File"),
	                                          parent,
	                                          GTK_FILE_CHOOSER_ACTION_SAVE, 
	                                          _("_Cancel"), GTK_RESPONSE_CANCEL,
	                                          _("_OK"), GTK_RESPONSE_ACCEPT,
	                                          NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(file_select), TRUE);

	if(fic_defaut != NULL)
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(file_select), fic_defaut);

	if(gtk_dialog_run(GTK_DIALOG(file_select)) == GTK_RESPONSE_ACCEPT)
	{
		gchar *fileName;
		gchar *msg;

		fileName = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_select));
		if ((!fileName || (strcmp(fileName, ""))) == 0)
		{
			msg = g_strdup_printf(_("File error\n"));
			show_message(msg, MSG_ERR);
			g_free(msg);
			g_free(fileName);
			gtk_widget_destroy(file_select);
			return;
		}

		Fic = fopen(fileName, "w");
		if(Fic == NULL)
		{
			msg = g_strdup_printf(_("Cannot open file %s: %s\n"), fileName, strerror(errno));
			show_message(msg, MSG_ERR);
			g_free(msg);
		}
		else
		{
			fic_defaut = g_strdup(fileName);

			write_buffer_with_func(write_file);

			fclose(Fic);
		}
		g_free(fileName);
	}
	gtk_widget_destroy(file_select);
}

gboolean gt_file_get_waiting_for_char (void)
{
  return waiting_for_char;
}

void gt_file_set_waiting_for_char (gboolean waiting)
{
  waiting_for_char = waiting;
}

const char *gt_file_get_default (void)
{
  return fic_defaut;
}

void gt_file_set_default (const char *file)
{
  g_free (fic_defaut);

  fic_defaut = g_strdup (file);
}
