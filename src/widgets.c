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

#include "term_config.h"
#include "fichier.h"
#include "serial-port.h"
#include "widgets.h"
#include "buffer.h"
#include "macros.h"
#include "logging.h"
#include "widgets.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined (__linux__)
#  include <asm/termios.h>       /* For control signals */
#endif
#if defined (__FreeBSD__) || defined (__FreeBSD_kernel__) \
     || defined (__NetBSD__) || defined (__NetBSD_kernel__) \
     || defined (__OpenBSD__) || defined (__OpenBSD_kernel__)
#  include <sys/ttycom.h>        /* For control signals */
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <vte/vte.h>

extern GtSerialPort *serial_port;
extern GtBuffer *buffer;
extern GtLogging *logger;

typedef enum _GtSerialSignalsWidgets {
    SIGNAL_RING,
    SIGNAL_DSR,
    SIGNAL_CD,
    SIGNAL_CTS,
    SIGNAL_RTS,
    SIGNAL_DTR,
    SIGNAL_COUNT
} GtSerialSignalsWidgets;

static guint signal_flags[] = {
    TIOCM_RI,
    TIOCM_DSR,
    TIOCM_CD,
    TIOCM_CTS,
    TIOCM_RTS,
    TIOCM_DTR
};

static char const *signal_names[] = {
    "RI",
    "DSR",
    "CD",
    "CTS",
    "RTS",
    "DTR",
    NULL
};

static guint id;
static gboolean echo_on;
static gboolean crlfauto_on;
static GtkWidget *status_bar;
static GtkWidget *signals[SIGNAL_COUNT];
static GtkWidget *Hex_Box;
static GtkWidget *scrolled_window;
static GtkWidget *popup_menu;
static GtkAccelGroup *shortcuts;
static GtkWidget *menu_bar;
static GtkWidget *main_vbox;
static GtkWidget *revealer;

static GtkBuilder *builder;

/* Exported variables */
GtkWidget *display = NULL;
GtkWidget *Fenetre;

/* Variables for hexadecimal display */
static guint bytes_per_line = 16;
static gchar blank_data[128];
static guint total_bytes;
static gboolean show_index = FALSE;

/* Local functions prototype */
static void signals_send_break_callback(GtkAction *action, gpointer data);
static void signals_toggle_DTR_callback(GtkAction *action, gpointer data);
static void signals_toggle_RTS_callback(GtkAction *action, gpointer data);
static void help_about_callback(GtkAction *action, gpointer data);
static void on_serial_port_signals_changed (GObject *object,
                                            GParamSpec *pspec,
                                            gpointer user_data);
static void on_serial_port_status_changed (GObject *object,
                                           GParamSpec *pspec,
                                           gpointer user_data);
static void on_window_destroyed (GtkWidget *widget, gpointer user_data);
static void initialize_hexadecimal_display(void);
static gboolean Send_Hexadecimal(GtkWidget *, GdkEventKey *, gpointer);
static gboolean pop_message(void);
static void Got_Input(VteTerminal *, gchar *, guint, gpointer);
static void edit_copy_callback(GtkAction *action, gpointer data);
static void update_copy_sensivity(VteTerminal *terminal, gpointer data);
static void edit_paste_callback(GtkAction *action, gpointer data);
static void edit_select_all_callback(GtkAction *action, gpointer data);

static void on_quit (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_clear_buffer (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_send_raw_file (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_save_raw_file (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_edit_copy_callback (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_edit_paste_callback (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_edit_select_all_callback (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_logging_start (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_logging_pause_resume (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_logging_stop (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_logging_clear (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_config_port (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_config_terminal (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_local_echo_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_config_macros (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_config_profile_select (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_config_profile_save (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_config_profile_delete (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_signals_send_break (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_signals_send_dtr (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_signals_send_rts (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_about (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_reconnect (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_action_toggle (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_action_radio (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_crlf_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_view_ascii_hex_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_view_index_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_view_send_hex_change_state  (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_view_hex_width_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_menubar_visibility_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_statusbar_visibility_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data);

const GActionEntry menu_actions[] = {
    /* File menu */
    {"clear", on_clear_buffer } ,
    {"send-file", on_send_raw_file },
    {"save-file", on_save_raw_file },
    {"quit", on_quit },

    /* Edit menu */
    {"copy", on_edit_copy_callback },
    {"paste", on_edit_paste_callback },
    {"select-all", on_edit_select_all_callback },

    /* Log menu */
    {"log.to-file", on_logging_start },
    {"log.pause-resume", on_action_toggle, NULL, "false", on_logging_pause_resume },
    {"log.stop", on_logging_stop },
    {"log.clear", on_logging_clear },

    /* Configuration menu */
    {"config.port", on_config_port },
    {"config.terminal", on_config_terminal },
    {"config.local-echo", on_action_toggle, NULL, "false", on_local_echo_change_state},
    {"config.crlf", on_action_toggle, NULL, "false", on_crlf_change_state },

    {"config.macros", on_config_macros },

    {"config.profile.select", on_config_profile_select },
    {"config.profile.save", on_config_profile_save },
    {"config.profile.delete", on_config_profile_delete },

    /* View menu */
    {"view.ascii-hex", on_action_radio, "s", "'ascii'", on_view_ascii_hex_change_state },
    {"view.index", on_action_toggle, NULL, "false", on_view_index_change_state },
    {"view.send-hex", on_action_toggle, NULL, "false", on_view_send_hex_change_state },
    {"view.hex-width", on_action_radio, "s", "'8'", on_view_hex_width_change_state },

    /* Signals menu */
    {"signals.send-break", on_signals_send_break },
    {"signals.send-dtr", on_signals_send_dtr },
    {"signals.send-rts", on_signals_send_rts },

    /* Help menu */
    {"about", on_about },

    /* Misc actions */
    { "reconnect", on_reconnect },
    { "menubar-visibility", on_action_toggle, NULL, "true", on_menubar_visibility_change_state },
    { "statusbar-visibility", on_action_toggle, NULL, "true", on_statusbar_visibility_change_state }
};

static GSimpleAction *find_action (const char *action)
{
    GActionGroup *group;

    group = gtk_widget_get_action_group (Fenetre, "main");

    return G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (group),
                                                        action));
}

static void on_serial_port_status_changed (GObject *object,
                                           GParamSpec *pspec,
                                           gpointer user_data)
{
    GError *error = gt_serial_port_get_last_error (GT_SERIAL_PORT (object));
    GtSerialPortState status = gt_serial_port_get_status (GT_SERIAL_PORT (object));
    char *message = NULL;

    if (status == GT_SERIAL_PORT_STATE_ERROR)
    {
        char *msg = NULL;
        if (error != NULL)
        {
            msg = g_strdup_printf (_("Serial port went to error: %s"),
                                   error->message);
        }
        else
        {
            msg = g_strdup (_("Serial port went to error. Reason unknown."));
        }

        g_warning ("%s", msg);
        show_message (msg, MSG_ERR);

        g_free (msg);
    }
    else if (status == GT_SERIAL_PORT_STATE_OFFLINE)
    {
        g_debug ("Serial port went offline");
    }
    else if (status == GT_SERIAL_PORT_STATE_ONLINE)
    {
        g_debug ("Serial port online");
    }

    message = gt_serial_port_to_string (serial_port);
    gt_main_window_set_status (message);
    Set_window_title(message);
    g_free(message);
}

void set_view(guint type)
{
    GActionGroup *group;
    GSimpleAction *show_index_action;
    GSimpleAction *action;

    group = gtk_widget_get_action_group (Fenetre, "main");
    action = G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (group), "view.hex-width"));

    show_index_action = G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (group),
                "view.index"));

    clear_display();
    switch(type)
    {
        case ASCII_VIEW:
            g_simple_action_set_enabled (show_index_action, FALSE);
            g_simple_action_set_enabled (action, FALSE);
            total_bytes = 0;
            gt_buffer_set_display_func (buffer, put_text);
            break;
        case HEXADECIMAL_VIEW:
            g_simple_action_set_enabled (show_index_action, TRUE);
            g_simple_action_set_enabled (action, TRUE);
            total_bytes = 0;
            gt_buffer_set_display_func (buffer, put_hexadecimal);
            break;
        default:
            gt_buffer_unset_display_func (buffer);
    }
    gt_buffer_write (buffer);
}

void Set_local_echo(gboolean echo)
{
  GActionGroup *group;
  GSimpleAction *action;
  GVariant *value;

  group = gtk_widget_get_action_group (Fenetre, "main");
  action = G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (group),
                            "config.local-echo"));

  echo_on = echo;

  value = g_variant_new_boolean (echo);
  g_variant_ref_sink (value);

  if(action)
      g_simple_action_set_state (action, value);

  g_variant_unref (value);
}

void Set_crlfauto(gboolean crlfauto)
{
  GActionGroup *group;
  GSimpleAction *action;
  GVariant *value;

  group = gtk_widget_get_action_group (Fenetre, "main");
  action = G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (group),
              "config.crlf"));

  crlfauto_on = crlfauto;

  value = g_variant_new_boolean (crlfauto);

  if(action)
      g_simple_action_set_state (action, value);

  g_variant_unref (value);
}

static gboolean terminal_button_press_callback(GtkWidget *widget,
                                        GdkEventButton *event,
                                        gpointer *data)
{

  if(event->type == GDK_BUTTON_PRESS &&
     event->button == 3 &&
     (event->state & gtk_accelerator_get_default_mod_mask()) == 0)
  {
#if GTK_CHECK_VERSION(3,22,0)
      gtk_menu_popup_at_pointer (GTK_MENU (popup_menu), (GdkEvent *) event);
#else
      gtk_menu_popup(GTK_MENU(popup_menu), NULL, NULL, NULL, NULL,
                     event->button, event->time);
#endif
      return TRUE;
  }

    return FALSE;
}

static void terminal_popup_menu_callback(GtkWidget *widget, gpointer data)
{
#if GTK_CHECK_VERSION(3,22,0)
    gtk_menu_popup_at_pointer (GTK_MENU (popup_menu), gtk_get_current_event ());
#else
    gtk_menu_popup(GTK_MENU(popup_menu), NULL, NULL, NULL, NULL,
                   0, gtk_get_current_event_time());
#endif
}

void create_main_window(GtkApplication *app)
{
  GtkWidget *label;
  GtkWidget *hex_send_entry;
  GActionGroup *group;
  int i = 0;


  group = G_ACTION_GROUP (g_simple_action_group_new ());
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   menu_actions,
                                   G_N_ELEMENTS (menu_actions),
                                   NULL);

  builder = gtk_builder_new_from_resource ("/org/jensge/Sellerie/main-window.ui");

  Fenetre = GTK_WIDGET (gtk_builder_get_object (builder, "window-main"));

  g_signal_connect (G_OBJECT (serial_port), "notify::status",
                    G_CALLBACK (on_serial_port_status_changed),
                    Fenetre);

  g_signal_connect (G_OBJECT (serial_port),
                    "notify::control",
                    G_CALLBACK (on_serial_port_signals_changed),
                    Fenetre);

  g_signal_connect (G_OBJECT (Fenetre), "destroy", G_CALLBACK (on_window_destroyed), NULL);
  gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW (Fenetre), TRUE);
  gtk_window_set_application (GTK_WINDOW (Fenetre), app);
  gtk_widget_insert_action_group (Fenetre, "main", group);

  g_object_bind_property (logger, "active", find_action ("log.to-file"), "enabled", G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE);
  g_object_bind_property(logger, "active", find_action ("log.pause-resume"), "enabled", G_BINDING_SYNC_CREATE);
  g_object_bind_property(logger, "active", find_action ("log.stop"), "enabled", G_BINDING_SYNC_CREATE);
  g_object_bind_property(logger, "active", find_action ("log.clear"), "enabled", G_BINDING_SYNC_CREATE);


  shortcuts = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(Fenetre), GTK_ACCEL_GROUP(shortcuts));

  
  Set_window_title("Sellerie");

  main_vbox = GTK_WIDGET (gtk_builder_get_object (builder, "box-main"));

  /* create vte window */
  display = vte_terminal_new();

  /* set terminal properties, these could probably be made user configurable */
  vte_terminal_set_scroll_on_output(VTE_TERMINAL(display), FALSE);
  vte_terminal_set_scroll_on_keystroke(VTE_TERMINAL(display), TRUE);
  vte_terminal_set_mouse_autohide(VTE_TERMINAL(display), TRUE);
  vte_terminal_set_backspace_binding(VTE_TERMINAL(display),
                                     VTE_ERASE_ASCII_BACKSPACE);

  clear_display();

  /* make vte window scrollable - inspired by gnome-terminal package */
  scrolled_window = gtk_scrolled_window_new(NULL, gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (display)));

  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);

  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window),
                                      GTK_SHADOW_NONE);

  revealer = gtk_revealer_new ();
  gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(display));
  gtk_box_pack_start (GTK_BOX (main_vbox), revealer, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(main_vbox), scrolled_window, TRUE, TRUE, 0);

  g_signal_connect(G_OBJECT(display), "button-press-event",
                   G_CALLBACK(terminal_button_press_callback), NULL);

  g_signal_connect(G_OBJECT(display), "popup-menu",
                   G_CALLBACK(terminal_popup_menu_callback), NULL);

  g_signal_connect(G_OBJECT(display), "selection-changed",
                   G_CALLBACK(update_copy_sensivity), NULL);
  update_copy_sensivity(VTE_TERMINAL(display), NULL);

  popup_menu = gtk_menu_new_from_model (G_MENU_MODEL (gtk_builder_get_object (builder, "popup-menu")));
  gtk_menu_attach_to_widget (GTK_MENU (popup_menu), display, NULL);

  /* send hex char box (hidden when not in use) */
  Hex_Box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  label = gtk_label_new(_("Hexadecimal data to send (separator : ';' or space) : "));
  gtk_box_pack_start(GTK_BOX(Hex_Box), label, FALSE, FALSE, 5);
  hex_send_entry = gtk_entry_new();
  g_signal_connect(GTK_WIDGET(hex_send_entry), "activate", (GCallback)Send_Hexadecimal, NULL);
  gtk_box_pack_start(GTK_BOX(Hex_Box), hex_send_entry, TRUE, TRUE, 5);
  gtk_box_pack_start(GTK_BOX(main_vbox), Hex_Box, FALSE, TRUE, 2);

  /* status bar */
  status_bar = gtk_statusbar_new();
  gtk_box_pack_start(GTK_BOX(main_vbox), status_bar, FALSE, FALSE, 0);
  id = gtk_statusbar_get_context_id(GTK_STATUSBAR(status_bar), "Messages");

  /* Set up serial signal indicators */
  for  (i = 0; i < SIGNAL_COUNT; i++) {
      label = gtk_label_new (signal_names[i]);
      gtk_box_pack_end(GTK_BOX(status_bar), label, FALSE, TRUE, 5);
      gtk_widget_set_sensitive(GTK_WIDGET(label), FALSE);
      signals[i] = label;
  }

  g_signal_connect_after(GTK_WIDGET(display), "commit", G_CALLBACK(Got_Input), NULL);

  gtk_window_set_default_size(GTK_WINDOW(Fenetre), 750, 550);
  gtk_widget_show_all(Fenetre);
  gtk_widget_hide(GTK_WIDGET(Hex_Box));
  gtk_application_add_window (app, GTK_WINDOW (Fenetre));
}

void initialize_hexadecimal_display(void)
{
  total_bytes = 0;
  memset(blank_data, ' ', 128);
  blank_data[bytes_per_line * 3 + 5] = 0;
}

void put_hexadecimal(gchar *string, guint size)
{
  static gchar data[128];
  static gchar data_byte[6];
  static guint bytes;
  glong column, row;

  guint i = 0;

  if(size == 0)
    return;

  while(i < size)
    {
      while(gtk_events_pending()) gtk_main_iteration();
      vte_terminal_get_cursor_position(VTE_TERMINAL(display), &column, &row);

      if(show_index)
	{
	  if(column == 0)
	    /* First byte on line */
	    {
	      sprintf(data, "%6d: ", total_bytes);
	      vte_terminal_feed(VTE_TERMINAL(display), data, strlen(data));
	      bytes = 0;
	    }
	}
      else
	{
	  if(column == 0)
	    bytes = 0;
	}

      /* Print hexadecimal characters */
      data[0] = 0;

      while(bytes < bytes_per_line && i < size)
	{
	  gint avance=0;
	  gchar ascii[1];

	  sprintf(data_byte, "%02X ", (guchar)string[i]);

        {
            GError *error = NULL;
            gt_logging_log(logger, data_byte, 3, NULL);
            if (error != NULL) {
                show_message(error->message, MSG_ERR);
                g_error_free(error);
            }
        }
	  vte_terminal_feed(VTE_TERMINAL(display), data_byte, 3);

	  avance = (bytes_per_line - bytes) * 3 + bytes + 2;

	  /* Move forward */
	  sprintf(data_byte, "%c[%dC", 27, avance);
	  vte_terminal_feed(VTE_TERMINAL(display), data_byte, strlen(data_byte));

	  /* Print ascii characters */
	  ascii[0] = (string[i] > 0x1F) ? string[i] : '.';
	  vte_terminal_feed(VTE_TERMINAL(display), ascii, 1);

	  /* Move backward */
	  sprintf(data_byte, "%c[%dD", 27, avance + 1);
	  vte_terminal_feed(VTE_TERMINAL(display), data_byte, strlen(data_byte));

	  if(bytes == bytes_per_line / 2 - 1)
	    vte_terminal_feed(VTE_TERMINAL(display), "- ", strlen("- "));

	  bytes++;
	  i++;

	  /* End of line ? */
	  if(bytes == bytes_per_line)
	    {
	      vte_terminal_feed(VTE_TERMINAL(display), "\r\n", 2);
	      total_bytes += bytes;
	    }

	}

    }
}

void put_text(gchar *string, guint size)
{
    GError *error = NULL;

    gt_logging_log (logger, string, size, &error);
    if (error != NULL) {
        show_message (error->message, MSG_ERR);
        g_error_free (error);
    }

    vte_terminal_feed(VTE_TERMINAL(display), string, size);
}

gint send_serial (gchar *string, gint len)
{
    gint bytes_written;

    bytes_written = gt_serial_port_send_chars (serial_port, string, len);
    if(bytes_written > 0)
    {
        if (echo_on)
            gt_buffer_put_chars (buffer, string, bytes_written, crlfauto_on);
    }

    return bytes_written;
}


static void Got_Input(VteTerminal *widget, gchar *text, guint length, gpointer ptr)
{
  send_serial(text, length);
}

void help_about_callback(GtkAction *action, gpointer data)
{
  const gchar *authors[] = {"Julien Schimtt", "Zach Davis", "Jens Georg", NULL};
  GdkPixbuf *logo = NULL;

  logo = gdk_pixbuf_new_from_resource_at_scale ("/org/jensge/Sellerie/org.jensge.Sellerie.svg",
                                                128, 128, TRUE, NULL);

  gtk_show_about_dialog(GTK_WINDOW(Fenetre),
                        "program-name", "Sellerie",
                        "version", VERSION,
                        "comments", _("Sellerie is a simple GTK+ terminal used to communicate with the serial port."),
                        "copyright", "Copyright © Julien Schmitt",
                        "authors", authors,
                        "website", "https://github.com/phako/gtkterm",
                        "website-label", "https://github.com/phako/gtkterm",
                        "license-type", GTK_LICENSE_GPL_3_0,
                        "logo", logo,
                        NULL);
  g_object_unref (logo);
}

static void on_serial_port_signals_changed (GObject *object,
                                            GParamSpec *pspec,
                                            gpointer user_data)
{
    guint port_signals = 0;
    int i = 0;

    port_signals = gt_serial_port_get_signals (GT_SERIAL_PORT (object));

    for (i = 0; i < SIGNAL_COUNT; i++) {
        gboolean active = (port_signals & signal_flags[i]) != 0;
        gtk_widget_set_sensitive (signals[i], active);
    }
}

static void on_window_destroyed (GtkWidget *widget, gpointer user_data)
{
    g_signal_handlers_disconnect_by_func (G_OBJECT (serial_port),
            G_CALLBACK (on_serial_port_status_changed), Fenetre);

    g_signal_handlers_disconnect_by_func (G_OBJECT (serial_port),
            G_CALLBACK (on_serial_port_signals_changed), Fenetre);
}

void signals_send_break_callback(GtkAction *action, gpointer data)
{
  gt_serial_port_send_brk (serial_port);
  Put_temp_message(_("Break signal sent!"), 800);
}

void signals_toggle_DTR_callback(GtkAction *action, gpointer data)
{
  gt_serial_port_set_signals (serial_port, 0);
}

void signals_toggle_RTS_callback(GtkAction *action, gpointer data)
{
  gt_serial_port_set_signals (serial_port, 1);
}

void gt_main_window_set_status (const char *msg)
{
    gt_main_window_pop_status ();
    gt_main_window_push_status (msg);
}

void gt_main_window_push_status (const char *msg)
{
    gtk_statusbar_push (GTK_STATUSBAR (status_bar), id, msg);
}

void gt_main_window_pop_status (void)
{
    gtk_statusbar_pop (GTK_STATUSBAR (status_bar), id);
}

void gt_main_window_add_shortcut (guint key, GdkModifierType mod, GClosure *closure)
{
    gtk_accel_group_connect (shortcuts, key, mod, GTK_ACCEL_MASK, closure);
}

void gt_main_window_remove_shortcut (GClosure *closure)
{
    gtk_accel_group_disconnect (shortcuts, closure);
}

void gt_main_window_set_info_bar (GtkWidget *widget)
{
    gtk_container_add (GTK_CONTAINER (revealer), widget);
    gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), TRUE);
}

void gt_main_window_remove_info_bar (GtkWidget *widget)
{
    gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), FALSE);
    gtk_container_remove (GTK_CONTAINER (revealer), widget);
}

void Set_window_title(const gchar *msg)
{
    gchar* header = g_strdup_printf("Sellerie - %s", msg);
    gtk_window_set_title(GTK_WINDOW(Fenetre), header);
    g_free(header);
}

void show_message(const gchar *message, gint type_msg)
{
 GtkWidget *Fenetre_msg;

 if(type_msg==MSG_ERR)
   {
     Fenetre_msg = gtk_message_dialog_new(GTK_WINDOW(Fenetre), 
					  GTK_DIALOG_DESTROY_WITH_PARENT, 
					  GTK_MESSAGE_ERROR, 
					  GTK_BUTTONS_OK, 
					  message, NULL);
   }
 else if(type_msg==MSG_WRN)
   {
     Fenetre_msg = gtk_message_dialog_new(GTK_WINDOW(Fenetre), 
					  GTK_DIALOG_DESTROY_WITH_PARENT, 
					  GTK_MESSAGE_WARNING, 
					  GTK_BUTTONS_OK, 
					  message, NULL);
   }
 else
   return;

 gtk_dialog_run(GTK_DIALOG(Fenetre_msg));
 gtk_widget_destroy(Fenetre_msg);
}

gboolean Send_Hexadecimal(GtkWidget *widget, GdkEventKey *event, gpointer pointer)
{
    guint i;
    gchar *text, *message, **tokens, *buff;
    guint scan_val;

    text = (gchar *)gtk_entry_get_text(GTK_ENTRY(widget));

    if(strlen(text) == 0){
        message = g_strdup_printf(_("Nothing sent."));
        Put_temp_message(message, 1500);
        gtk_entry_set_text(GTK_ENTRY(widget), "");
        g_free(message);
        return FALSE;
    }

    tokens = g_strsplit_set(text, " ;", -1);
    buff = g_malloc(g_strv_length(tokens));

    for(i = 0; tokens[i] != NULL; i++){
        if(sscanf(tokens[i], "%02X", &scan_val) != 1){
            Put_temp_message(_("Improper formatted hex input, 0 bytes sent!"),
                             1500);
            g_free(buff);
            return FALSE;
        }
        buff[i] = scan_val;
    }

    send_serial(buff, i);
    g_free(buff);

    message = g_strdup_printf(ngettext("%d byte sent.", "%d bytes sent.", i), i);
    Put_temp_message(message, 2000);
    gtk_entry_set_text(GTK_ENTRY(widget), "");
    g_strfreev(tokens);

    return FALSE;
}

void Put_temp_message(const gchar *text, gint time)
{
  /* time in ms */
  gtk_statusbar_push(GTK_STATUSBAR(status_bar), id, text);
  g_timeout_add(time, (GSourceFunc)pop_message, NULL);
}

gboolean pop_message(void)
{
  gtk_statusbar_pop(GTK_STATUSBAR(status_bar), id);

  return FALSE;
}

void clear_display(void)
{
  initialize_hexadecimal_display();
  if(display)
    vte_terminal_reset(VTE_TERMINAL(display), TRUE, TRUE);
}

void edit_copy_callback(GtkAction *action, gpointer data)
{
  vte_terminal_copy_clipboard(VTE_TERMINAL(display));
}

void update_copy_sensivity(VteTerminal *terminal, gpointer data)
{
  GActionGroup *group;
  GAction *action;
  gboolean can_copy;

  can_copy = vte_terminal_get_has_selection(VTE_TERMINAL(terminal));

  group = gtk_widget_get_action_group (Fenetre, "main");
  action = g_action_map_lookup_action (G_ACTION_MAP (group), "copy");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_copy);
}

void edit_paste_callback(GtkAction *action, gpointer data)
{
  vte_terminal_paste_clipboard(VTE_TERMINAL(display));
}

void edit_select_all_callback(GtkAction *action, gpointer data)
{
  vte_terminal_select_all(VTE_TERMINAL(display));
}


void on_quit (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    gtk_window_close(GTK_WINDOW (Fenetre));
}

void on_clear_buffer (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    gt_buffer_clear (buffer);
    clear_display();
}

void on_send_raw_file (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    send_raw_file (GTK_WINDOW (Fenetre));
}

void on_save_raw_file (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    save_raw_file (GTK_WINDOW (Fenetre));
}

void on_edit_copy_callback (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    edit_copy_callback (NULL, NULL);
}

void on_edit_paste_callback (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    edit_paste_callback (NULL, NULL);
}

void on_edit_select_all_callback (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    edit_select_all_callback (NULL, NULL);
}

void on_logging_start (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkWidget *file_select;
    gint retval = GTK_RESPONSE_NONE;

    file_select = gtk_file_chooser_dialog_new(_("Log file selection"),
                                       GTK_WINDOW(user_data),
                                       GTK_FILE_CHOOSER_ACTION_SAVE,
                                       _("_Cancel"), GTK_RESPONSE_CANCEL,
                                       _("_OK"), GTK_RESPONSE_OK, NULL);
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER(file_select), TRUE);

    const char *default_file = gt_logging_get_default_file(logger);
    if (default_file != NULL) {
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(file_select), default_file);
    }

    retval = gtk_dialog_run(GTK_DIALOG(file_select));
    gtk_widget_hide(file_select);
    if (retval == GTK_RESPONSE_OK) {
        GError *error = NULL;
        char *file_name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (file_select));
        gt_logging_start (logger, file_name, &error);
        if (error != NULL) {
            show_message(error->message, MSG_ERR);
            g_error_free(error);
        }
        g_free(file_name);
    }
}

void on_logging_pause_resume (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    gt_logging_pause_resume (logger);
    g_simple_action_set_state (action, parameter);
}

void on_logging_stop (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    gt_logging_stop (logger);
}

void on_logging_clear (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GError *error = NULL;

    gt_logging_clear (logger, &error);
    if (error != NULL) {
        show_message(error->message, MSG_ERR);
        g_error_free(error);
    }
}

void on_config_port (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    Config_Port_Fenetre (GTK_WINDOW (Fenetre));
}

void on_config_terminal (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    Config_Terminal (NULL, NULL);
}

void on_config_macros (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    Config_macros (GTK_WINDOW (Fenetre));
}

void on_config_profile_select (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    select_config_callback (NULL, NULL);
}

void on_config_profile_save (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    save_config_callback (NULL, NULL);
}

void on_config_profile_delete (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    delete_config_callback (NULL, NULL);
}

void on_signals_send_break (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    signals_send_break_callback (NULL, NULL);
}

void on_signals_send_dtr (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    signals_toggle_DTR_callback (NULL, NULL);
}

void on_signals_send_rts (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    signals_toggle_RTS_callback (NULL, NULL);
}

void on_about (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    help_about_callback (NULL, NULL);
}

void on_reconnect (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    gt_serial_port_reconnect (serial_port);
}

void on_action_toggle (GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    GVariant *state;

    /* Toggle the current state. */
    state = g_action_get_state (G_ACTION (action));
    g_action_change_state (G_ACTION (action),
                           g_variant_new_boolean (!g_variant_get_boolean (state)));
    g_variant_unref (state);
}

extern GtSerialPortConfiguration config;

void on_local_echo_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    echo_on = g_variant_get_boolean (parameter);
    config.echo = echo_on;
    gt_serial_port_set_local_echo (serial_port, config.echo);
    g_simple_action_set_state (action, parameter);
}

void on_crlf_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    crlfauto_on = g_variant_get_boolean (parameter);
    config.crlfauto = crlfauto_on;
    gt_serial_port_set_crlfauto (serial_port, crlfauto_on);
    g_simple_action_set_state (action, parameter);
}

void on_view_index_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    show_index = g_variant_get_boolean (parameter);
    set_view(HEXADECIMAL_VIEW);
    g_simple_action_set_state (action, parameter);
}

void on_view_send_hex_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    gtk_widget_set_visible (Hex_Box, g_variant_get_boolean (parameter));

    g_simple_action_set_state (action, parameter);
}

void on_menubar_visibility_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    gtk_widget_set_visible (menu_bar, g_variant_get_boolean (parameter));

    g_simple_action_set_state (action, parameter);
}

void on_statusbar_visibility_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    gtk_widget_set_visible (status_bar, g_variant_get_boolean (parameter));

    g_simple_action_set_state (action, parameter);
}

void on_action_radio (GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    g_action_change_state (G_ACTION (action), parameter);
}

static void on_view_ascii_hex_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    gsize length = 0;
    const char *value = g_variant_get_string (parameter, &length);
    if (strncmp (value, "hex", length) == 0) {
        set_view (HEXADECIMAL_VIEW);
    } else if (strncmp (value, "ascii", length) == 0) {
        set_view (ASCII_VIEW);
    }

    g_simple_action_set_state (action, parameter);
}

static void on_view_hex_width_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    gsize length = 0;
    const char *value = g_variant_get_string (parameter, &length);
    gint current_value = atoi (value);

    bytes_per_line = current_value;
    set_view (HEXADECIMAL_VIEW);

    g_simple_action_set_state (action, parameter);
}
