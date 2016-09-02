/***********************************************************************/
/* widgets.c                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Functions for the management of the GUI for the main window    */
/*                                                                     */
/*   ChangeLog                                                         */
/*   (All changes by Julien Schmitt except when explicitly written)    */
/*                                                                     */
/*      - 0.99.7 : Changed keyboard shortcuts to <ctrl><shift>         */ 
/*	            (Ken Peek)                                         */
/*      - 0.99.6 : Added scrollbar and copy/paste (Zach Davis)         */
/*                                                                     */
/*      - 0.99.5 : Make package buildable on pure *BSD by changing the */
/*                 include to asm/termios.h by sys/ttycom.h            */
/*                 Print message without converting it into the locale */
/*                 in show_message()                                   */
/*                 Set backspace key binding to backspace so that the  */
/*                 backspace works. It would even be nicer if the      */
/*                 behaviour of this key could be configured !         */
/*      - 0.99.4 : - Sebastien Bacher -                                */
/*                 Added functions for CR LF auto mode                 */
/*                 Fixed put_text() to have \r\n for the VTE Widget    */
/*                 Rewritten put_hexadecimal() function                */
/*                 - Julien -                                          */
/*                 Modified send_serial to return the actual number of */
/*                 bytes written, and also only display exactly what   */
/*                 is written                                          */
/*      - 0.99.3 : Modified to use a VTE terminal                      */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.99.0 : \b byte now handled correctly by the ascii widget   */
/*                 SUPPR (0x7F) also prints correctly                  */
/*                 adapted for macros                                  */
/*                 modified "about" dialog                             */
/*      - 0.98.6 : fixed possible buffer overrun in hex send           */
/*                 new "Send break" option                             */
/*      - 0.98.5 : icons in the menu                                   */
/*                 bug fixed with local echo and hexadecimal           */
/*                 modified hexadecimal send separator, and bug fixed  */
/*      - 0.98.4 : new hexadecimal display / send                      */
/*      - 0.98.3 : put_text() modified to fit with 0x0D 0x0A           */
/*      - 0.98.2 : added local echo by Julien                          */
/*      - 0.98 : file creation by Julien                               */
/*                                                                     */
/***********************************************************************/

#include <gtk/gtk.h>
#if defined (__linux__)
#  include <asm/termios.h>       /* For control signals */
#endif
#if defined (__FreeBSD__) || defined (__FreeBSD_kernel__) \
     || defined (__NetBSD__) || defined (__NetBSD_kernel__) \
     || defined (__OpenBSD__) || defined (__OpenBSD_kernel__)
#  include <sys/ttycom.h>        /* For control signals */
#endif
#include <vte/vte.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "term_config.h"
#include "fichier.h"
#include "serie.h"
#include "widgets.h"
#include "buffer.h"
#include "macros.h"
#include "auto_config.h"
#include "logging.h"
#include "widgets.h"

#include <config.h>
#include <glib/gi18n.h>

guint id;
gboolean echo_on;
gboolean crlfauto_on;
GtkWidget *StatusBar;
GtkWidget *signals[6];
static GtkWidget *Hex_Box;
GtkWidget *scrolled_window;
GtkWidget *Fenetre;
GtkWidget *popup_menu;
GtkUIManager *ui_manager;
GtkAccelGroup *shortcuts;
GtkActionGroup *action_group;
GtkWidget *display = NULL;

GtkWidget *Text;
GtkTextBuffer *buffer;
GtkTextIter iter;

static GtkBuilder *builder;

/* Variables for hexadecimal display */
static guint bytes_per_line = 16;
static gchar blank_data[128];
static guint total_bytes;
static gboolean show_index = FALSE;

/* Local functions prototype */
void signals_send_break_callback(GtkAction *action, gpointer data);
void signals_toggle_DTR_callback(GtkAction *action, gpointer data);
void signals_toggle_RTS_callback(GtkAction *action, gpointer data);
void help_about_callback(GtkAction *action, gpointer data);
gboolean Envoie_car(GtkWidget *, GdkEventKey *, gpointer);
gboolean control_signals_read(void);
void CR_LF_auto_toggled_callback(GtkAction *action, gpointer data);
void initialize_hexadecimal_display(void);
gboolean Send_Hexadecimal(GtkWidget *, GdkEventKey *, gpointer);
gboolean pop_message(void);
static void Got_Input(VteTerminal *, gchar *, guint, gpointer);
void edit_copy_callback(GtkAction *action, gpointer data);
void update_copy_sensivity(VteTerminal *terminal, gpointer data);
void edit_paste_callback(GtkAction *action, gpointer data);
void edit_select_all_callback(GtkAction *action, gpointer data);

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
static void on_action_toggle (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_action_radio (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_crlf_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_view_ascii_hex_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_view_index_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_view_send_hex_change_state  (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void on_view_hex_width_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data);

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
    {"about", on_about }
};

static GSimpleAction *find_action (const char *action)
{
    GActionGroup *group;

    group = gtk_widget_get_action_group (Fenetre, "main");

    return G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (group),
                                                        action));
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
  set_clear_func(clear_display);
  switch(type)
    {
    case ASCII_VIEW:
      g_simple_action_set_enabled (show_index_action, FALSE);
      g_simple_action_set_enabled (action, FALSE);
      total_bytes = 0;
      set_display_func(put_text);
      break;
    case HEXADECIMAL_VIEW:
      g_simple_action_set_enabled (show_index_action, TRUE);
      g_simple_action_set_enabled (action, TRUE);
      total_bytes = 0;
      set_display_func(put_hexadecimal);
      break;
    default:
      set_display_func(NULL);
    }
  write_buffer();
}

void Set_local_echo(gboolean echo)
{
  GActionGroup *group;
  GSimpleAction *action;
  GVariant *value;

  group = gtk_widget_get_action_group (Fenetre, "main");
  action = G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (group),
                            "config.logal-echo"));

  echo_on = echo;

  value = g_variant_new_boolean (echo);

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

void toggle_logging_sensitivity(gboolean currentlyLogging)
{
    g_simple_action_set_enabled (find_action ("log.to-file"), !currentlyLogging);
    g_simple_action_set_enabled (find_action ("log.pause-resume"),
                                 currentlyLogging);
    g_simple_action_set_enabled (find_action ("log.stop"), currentlyLogging);
    g_simple_action_set_enabled (find_action ("log.clear"), currentlyLogging);
}

static gboolean terminal_button_press_callback(GtkWidget *widget,
                                        GdkEventButton *event,
                                        gpointer *data)
{

  if(event->type == GDK_BUTTON_PRESS &&
     event->button == 3 &&
     (event->state & gtk_accelerator_get_default_mod_mask()) == 0)
  {
      gtk_menu_popup(GTK_MENU(popup_menu), NULL, NULL, NULL, NULL,
                     event->button, event->time);
      return TRUE;
  }

    return FALSE;
}

static void terminal_popup_menu_callback(GtkWidget *widget, gpointer data)
{
  gtk_menu_popup(GTK_MENU(popup_menu), NULL, NULL, NULL, NULL,
                 0, gtk_get_current_event_time());
}

void create_main_window(void)
{
  GtkWidget *main_vbox, *label;
  GtkWidget *hex_send_entry;
  GActionGroup *group;

  group = G_ACTION_GROUP (g_simple_action_group_new ());
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   menu_actions,
                                   G_N_ELEMENTS (menu_actions),
                                   NULL);

  builder = gtk_builder_new_from_resource ("/org/jensge/GtkTerm/main-window.ui");

  Fenetre = GTK_WIDGET (gtk_builder_get_object (builder, "window-main"));
  gtk_widget_insert_action_group (Fenetre, "main", group);

  shortcuts = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(Fenetre), GTK_ACCEL_GROUP(shortcuts));

  g_signal_connect(GTK_WIDGET(Fenetre), "destroy", (GCallback)gtk_main_quit, NULL);
  g_signal_connect(GTK_WIDGET(Fenetre), "delete_event", (GCallback)gtk_main_quit, NULL);
  
  Set_window_title("GtkTerm");

  main_vbox = GTK_WIDGET (gtk_builder_get_object (builder, "box-main"));

  {
      gtk_box_pack_start (GTK_BOX (main_vbox),
              gtk_menu_bar_new_from_model (
                  G_MENU_MODEL (gtk_builder_get_object (builder, "window-menu"))),
              FALSE, FALSE, 0);
  }

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

  gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(display));

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

  /* set up logging buttons availability */
  toggle_logging_sensitivity(FALSE);

  /* send hex char box (hidden when not in use) */
  Hex_Box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  label = gtk_label_new(_("Hexadecimal data to send (separator : ';' or space) : "));
  gtk_box_pack_start(GTK_BOX(Hex_Box), label, FALSE, FALSE, 5);
  hex_send_entry = gtk_entry_new();
  g_signal_connect(GTK_WIDGET(hex_send_entry), "activate", (GCallback)Send_Hexadecimal, NULL);
  gtk_box_pack_start(GTK_BOX(Hex_Box), hex_send_entry, TRUE, TRUE, 5);
  gtk_box_pack_start(GTK_BOX(main_vbox), Hex_Box, FALSE, TRUE, 2);

  /* status bar */
  StatusBar = gtk_statusbar_new();
  gtk_box_pack_start(GTK_BOX(main_vbox), StatusBar, FALSE, FALSE, 0);
  id = gtk_statusbar_get_context_id(GTK_STATUSBAR(StatusBar), "Messages");

  label = gtk_label_new("RI");
  gtk_box_pack_end(GTK_BOX(StatusBar), label, FALSE, TRUE, 5);
  gtk_widget_set_sensitive(GTK_WIDGET(label), FALSE);
  signals[0] = label;

  label = gtk_label_new("DSR");
  gtk_box_pack_end(GTK_BOX(StatusBar), label, FALSE, TRUE, 5);
  signals[1] = label;

  label = gtk_label_new("CD");
  gtk_box_pack_end(GTK_BOX(StatusBar), label, FALSE, TRUE, 5);
  signals[2] = label;

  label = gtk_label_new("CTS");
  gtk_box_pack_end(GTK_BOX(StatusBar), label, FALSE, TRUE, 5);
  signals[3] = label;

  label = gtk_label_new("RTS");
  gtk_box_pack_end(GTK_BOX(StatusBar), label, FALSE, TRUE, 5);
  signals[4] = label;

  label = gtk_label_new("DTR");
  gtk_box_pack_end(GTK_BOX(StatusBar), label, FALSE, TRUE, 5);
  signals[5] = label;

  g_signal_connect_after(GTK_WIDGET(display), "commit", G_CALLBACK(Got_Input), NULL);

  g_timeout_add(POLL_DELAY, (GSourceFunc)control_signals_read, NULL);

  gtk_window_set_default_size(GTK_WINDOW(Fenetre), 750, 550);
  gtk_widget_show_all(Fenetre);
  gtk_widget_hide(GTK_WIDGET(Hex_Box));
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
	  log_chars(data_byte, 3);
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
    log_chars(string, size);
    vte_terminal_feed(VTE_TERMINAL(display), string, size);
}

gint send_serial(gchar *string, gint len)
{
  gint bytes_written;

  bytes_written = Send_chars(string, len);
  if(bytes_written > 0)
    {
      if(echo_on)
	  put_chars(string, bytes_written, crlfauto_on);
    }

  return bytes_written;
}


static void Got_Input(VteTerminal *widget, gchar *text, guint length, gpointer ptr)
{
  send_serial(text, length);
}

gboolean Envoie_car(GtkWidget *widget, GdkEventKey *event, gpointer pointer)
{
  if(g_utf8_validate(event->string, 1, NULL))
    send_serial(event->string, 1);

  return FALSE;
}


void help_about_callback(GtkAction *action, gpointer data)
{
  const gchar *authors[] = {"Julien Schimtt", "Zach Davis", "Jens Georg", NULL};
  GdkPixbuf *logo = NULL;

  logo = gdk_pixbuf_new_from_resource_at_scale ("/org/jensge/GtkTerm/Serial-port.svg",
                                                128, 128, TRUE, NULL);

  gtk_show_about_dialog(GTK_WINDOW(Fenetre),
                        "program-name", "GTKTerm-lzr",
                        "version", VERSION,
                        "comments", _("GTKTerm-lzr is a simple GTK+ terminal used to communicate with the serial port."),
                        "copyright", "Copyright © Julien Schimtt",
                        "authors", authors,
                        "website", "https://github.com/phako/gtkterm",
                        "website-label", "https://github.com/phako/gtkterm",
                        "license-type", GTK_LICENSE_LGPL_3_0,
                        "logo", logo,
                        NULL);
  g_object_unref (logo);
}

static void show_control_signals(int stat)
{
  if(stat & TIOCM_RI)
    gtk_widget_set_sensitive(GTK_WIDGET(signals[0]), TRUE);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(signals[0]), FALSE);
  if(stat & TIOCM_DSR)
    gtk_widget_set_sensitive(GTK_WIDGET(signals[1]), TRUE);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(signals[1]), FALSE);
  if(stat & TIOCM_CD)
    gtk_widget_set_sensitive(GTK_WIDGET(signals[2]), TRUE);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(signals[2]), FALSE);
  if(stat & TIOCM_CTS)
    gtk_widget_set_sensitive(GTK_WIDGET(signals[3]), TRUE);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(signals[3]), FALSE);
  if(stat & TIOCM_RTS)
    gtk_widget_set_sensitive(GTK_WIDGET(signals[4]), TRUE);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(signals[4]), FALSE);
  if(stat & TIOCM_DTR)
    gtk_widget_set_sensitive(GTK_WIDGET(signals[5]), TRUE);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(signals[5]), FALSE);
}

void signals_send_break_callback(GtkAction *action, gpointer data)
{
  sendbreak();
  Put_temp_message(_("Break signal sent!"), 800);
}

void signals_toggle_DTR_callback(GtkAction *action, gpointer data)
{
  Set_signals(0);
}

void signals_toggle_RTS_callback(GtkAction *action, gpointer data)
{
  Set_signals(1);
}

gboolean control_signals_read(void)
{
  int state;

  state = lis_sig();
  if(state >= 0)
    show_control_signals(state);

  return TRUE;
}

void gt_main_window_set_status (const char *msg)
{
    gt_main_window_pop_status ();
    gt_main_window_push_status (msg);
}

void gt_main_window_push_status (const char *msg)
{
    gtk_statusbar_push (GTK_STATUSBAR (StatusBar), id, msg);
}

void gt_main_window_pop_status (void)
{
    gtk_statusbar_pop (GTK_STATUSBAR (StatusBar), id);
}

void gt_main_window_add_shortcut (guint key, GdkModifierType mod, GClosure *closure)
{
    gtk_accel_group_connect (shortcuts, key, mod, GTK_ACCEL_MASK, closure);
}

void gt_main_window_remove_shortcut (GClosure *closure)
{
    gtk_accel_group_disconnect (shortcuts, closure);
}

void Set_window_title(const gchar *msg)
{
    gchar* header = g_strdup_printf("GtkTerm - %s", msg);
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
        message = g_strdup_printf(_("0 byte(s) sent!"));
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

    message = g_strdup_printf(_("%d byte(s) sent!"), i);
    Put_temp_message(message, 2000);
    gtk_entry_set_text(GTK_ENTRY(widget), "");
    g_strfreev(tokens);

    return FALSE;
}

void Put_temp_message(const gchar *text, gint time)
{
  /* time in ms */
  gtk_statusbar_push(GTK_STATUSBAR(StatusBar), id, text);
  g_timeout_add(time, (GSourceFunc)pop_message, NULL);
}

gboolean pop_message(void)
{
  gtk_statusbar_pop(GTK_STATUSBAR(StatusBar), id);

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
    gtk_main_quit ();
}

void on_clear_buffer (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    clear_buffer ();
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
    logging_start (GTK_WINDOW (Fenetre));
}

void on_logging_pause_resume (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    logging_pause_resume ();
    g_simple_action_set_state (action, parameter);
}

void on_logging_stop (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    logging_stop ();
}

void on_logging_clear (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    logging_clear ();
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

void on_action_toggle (GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    GVariant *state;

    /* Toggle the current state. */
    state = g_action_get_state (G_ACTION (action));
    g_action_change_state (G_ACTION (action),
                           g_variant_new_boolean (!g_variant_get_boolean (state)));
    g_variant_unref (state);
}

void on_local_echo_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    configure_echo(g_variant_get_boolean (parameter));
    g_simple_action_set_state (action, parameter);
}

void on_crlf_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    crlfauto_on = g_variant_get_boolean (parameter);
    configure_crlfauto (crlfauto_on);
    g_simple_action_set_state (action, parameter);
}

void on_view_index_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    show_index = g_variant_get_boolean (parameter);
    set_view(HEXADECIMAL_VIEW);
    g_simple_action_set_state (action, parameter);
}

void on_view_send_hex_change_state (GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    gboolean show_hex_view = g_variant_get_boolean (parameter);
    if (show_hex_view) {
        gtk_widget_show (GTK_WIDGET (Hex_Box));
    } else {
        gtk_widget_hide (GTK_WIDGET (Hex_Box));
    }

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
