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

#include "file-transfer.h"
#include "infobar.h"
#include "macro-editor.h"
#include "main-window.h"
#include "serial-view.h"
#include "term_config.h"
#include "view-config.h"
#include "macro-manager.h"

#include <stdlib.h>

#include <glib/gi18n.h>

#if defined(__linux__)
#include <asm/termios.h> /* For control signals */
#endif
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) ||                     \
    defined(__NetBSD__) || defined(__NetBSD_kernel__) ||                       \
    defined(__OpenBSD__) || defined(__OpenBSD_kernel__)
#include <sys/ttycom.h> /* For control signals */
#endif

extern GtSerialPortConfiguration config;

G_DEFINE_TYPE (GtMainWindow, gt_main_window, GTK_TYPE_APPLICATION_WINDOW)

enum { PROP_0, N_PROPS };

typedef enum {
    GT_MAIN_WINDOW_VIEW_TYPE_ASCII,
    GT_MAIN_WINDOW_VIEW_TYPE_HEX
} GtMainWindowViewType;

static guint signal_flags[] = {
    TIOCM_RI, TIOCM_DSR, TIOCM_CD, TIOCM_CTS, TIOCM_RTS, TIOCM_DTR};

static char const *signal_names[] = {
    "RI", "DSR", "CD", "CTS", "RTS", "DTR", NULL};

/* Private functions */
static void
gt_main_window_set_view (GtMainWindow *self, GtMainWindowViewType type);

static void
gt_main_window_clear_display (GtMainWindow *self);

/* Call-backs */
static void
on_serial_port_status_changed (GObject *object,
                               GParamSpec *pspec,
                               gpointer user_data);

static void
on_serial_port_signals_changed (GObject *object,
                                GParamSpec *pspec,
                                gpointer user_data);

static void
on_serial_port_data_available (GtMainWindow *self,
                               GBytes *bytes,
                               gpointer user_data);

static void
on_vte_button_press_callback (
    GtkGesture *click, int n_press, gdouble x, gdouble y, gpointer user_data);

static void
on_send_hexadecimal (GtkWidget *widget, gpointer pointer);

static void
on_vte_commit (VteTerminal *widget, gchar *text, guint length, gpointer ptr);

static void
on_display_updated (GtMainWindow *self,
                    gchar *text,
                    guint length,
                    gpointer user_data);

static void
on_action_about (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer user_data);

static void
on_edit_select_all_callback (GSimpleAction *action,
                             GVariant *parameter,
                             gpointer user_data);

static void
on_edit_paste_callback (GSimpleAction *action,
                        GVariant *parameter,
                        gpointer user_data);
static void
on_edit_copy_callback (GSimpleAction *action,
                       GVariant *parameter,
                       gpointer user_data);

static void
on_selection_changed (VteTerminal *terminal, gpointer data);

static void
on_signals_send_break (GSimpleAction *action,
                       GVariant *parameter,
                       gpointer user_data);

static void
on_signals_send_dtr (GSimpleAction *action,
                     GVariant *parameter,
                     gpointer user_data);

static void
on_signals_send_rts (GSimpleAction *action,
                     GVariant *parameter,
                     gpointer user_data);

static void
on_reconnect (GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void
on_macro (GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void
on_quit (GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void
on_local_echo_changed (GObject *gobject, GParamSpec *pspec, gpointer user_data);

static void
on_crlf_changed (GObject *gobject, GParamSpec *pspec, gpointer user_data);

static void
on_logging_start (GSimpleAction *action,
                  GVariant *parameter,
                  gpointer user_data);
static void
on_logging_pause_resume (GSimpleAction *action,
                         GVariant *parameter,
                         gpointer user_data);
static void
on_logging_stop (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer user_data);
static void
on_logging_clear (GSimpleAction *action,
                  GVariant *parameter,
                  gpointer user_data);
static void
on_config_port (GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void
on_clear_buffer (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer user_data);

static void
on_action_radio (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer user_data);
static void
on_view_ascii_hex_change_state (GSimpleAction *action,
                                GVariant *parameter,
                                gpointer user_data);
static void
on_view_index_change_state (GSimpleAction *action,
                            GVariant *parameter,
                            gpointer user_data);
static void
on_view_hex_width_change_state (GSimpleAction *action,
                                GVariant *parameter,
                                gpointer user_data);

static void
on_send_raw_file (GSimpleAction *action,
                  GVariant *parameter,
                  gpointer user_data);
static void
on_save_raw_file (GSimpleAction *action,
                  GVariant *parameter,
                  gpointer user_data);

static void
on_config_terminal (GSimpleAction *action,
                    GVariant *parameter,
                    gpointer user_data);
static void
on_config_macros (GSimpleAction *action,
                  GVariant *parameter,
                  gpointer user_data);
static void
on_config_profile_select (GSimpleAction *action,
                          GVariant *parameter,
                          gpointer user_data);
static void
on_config_profile_save (GSimpleAction *action,
                        GVariant *parameter,
                        gpointer user_data);
static void
on_config_profile_delete (GSimpleAction *action,
                          GVariant *parameter,
                          gpointer user_data);

static const GActionEntry actions[] = {
    /* File menu */
    {"clear", on_clear_buffer},
    {"send-file", on_send_raw_file},
    {"save-file", on_save_raw_file},
    {"quit", on_quit},

    /* Edit menu */
    {"select-all", on_edit_select_all_callback},
    {"copy", on_edit_copy_callback},
    {"paste", on_edit_paste_callback},

    /* Log menu */
    {"log.to-file", on_logging_start},
    {"log.pause-resume", NULL, NULL, "false", on_logging_pause_resume},
    {"log.stop", on_logging_stop},
    {"log.clear", on_logging_clear},

    /* Signals menu */
    {"signals.send-break", on_signals_send_break},
    {"signals.send-dtr", on_signals_send_dtr},
    {"signals.send-rts", on_signals_send_rts},

    /* Configuration menu */
    {"config.port", on_config_port},

    /* View menu */
    {"view.ascii-hex",
     on_action_radio,
     "s",
     "'ascii'",
     on_view_ascii_hex_change_state},
    {"view.index", NULL, NULL, "false", on_view_index_change_state},
    {"view.hex-width",
     on_action_radio,
     "s",
     "'8'",
     on_view_hex_width_change_state},
    /* Help menu */
    {"about", on_action_about},

    // Misc actions
    {"reconnect", on_reconnect},
    {"macro", NULL, "s", "''", on_macro},

    /* File menu */
    {"config.terminal", on_config_terminal},

    {"config.macros", on_config_macros},

    {"config.profile.select", on_config_profile_select},
    {"config.profile.save", on_config_profile_save},
    {"config.profile.delete", on_config_profile_delete},
};

// static GParamSpec *properties[N_PROPS];

GtkWidget *
gt_main_window_new (GtkApplication *app)
{
    return GTK_WIDGET (g_object_new (
        GT_TYPE_MAIN_WINDOW, "application", app, "show-menubar", TRUE, NULL));
}

static void
gt_main_window_dispose (GObject *object)
{
    GtMainWindow *self = GT_MAIN_WINDOW (object);

    gt_serial_port_close_and_unlock (self->serial_port);

    g_clear_object (&self->serial_port);
    g_clear_object (&self->buffer);
    g_clear_object (&self->logger);
    g_clear_object (&self->shortcuts);

    G_OBJECT_CLASS (gt_main_window_parent_class)->dispose (object);
}

static void
gt_main_window_finalize (GObject *object)
{
    GtMainWindow *self = (GtMainWindow *)object;

    g_clear_pointer (&self->default_raw_file, g_free);
    G_OBJECT_CLASS (gt_main_window_parent_class)->finalize (object);
}

static void
gt_main_window_get_property (GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    //    GtMainWindow *self = GT_MAIN_WINDOW (object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gt_main_window_set_property (GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    //    GtMainWindow *self = GT_MAIN_WINDOW (object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gt_main_window_class_init (GtMainWindowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = gt_main_window_dispose;
    object_class->finalize = gt_main_window_finalize;
    object_class->get_property = gt_main_window_get_property;
    object_class->set_property = gt_main_window_set_property;

    gtk_widget_class_set_template_from_resource (
        widget_class, "/org/jensge/Sellerie/main-window.ui");
    gtk_widget_class_bind_template_child (widget_class, GtMainWindow, main_box);
    gtk_widget_class_bind_template_child (widget_class, GtMainWindow, hex_box);
    gtk_widget_class_bind_template_child (
        widget_class, GtMainWindow, status_bar);
    gtk_widget_class_bind_template_child (
        widget_class, GtMainWindow, status_box);
    gtk_widget_class_bind_template_child (
        widget_class, GtMainWindow, scrolled_window);
    gtk_widget_class_bind_template_child (
        widget_class, GtMainWindow, hex_send_entry);
    gtk_widget_class_bind_template_child (widget_class, GtMainWindow, revealer);
    gtk_widget_class_bind_template_child (
        widget_class, GtMainWindow, popup_menu_model);
}
static void
gt_main_window_init (GtMainWindow *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
    gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW (self),
                                             TRUE);


    self->buffer = gt_buffer_new ();
    self->serial_port = gt_serial_port_new ();
    self->logger = gt_logging_new ();

    self->group = G_ACTION_GROUP (g_simple_action_group_new ());
    g_action_map_add_action_entries (
        G_ACTION_MAP (self->group), actions, G_N_ELEMENTS (actions), self);
    gtk_widget_insert_action_group (GTK_WIDGET (self), "main", self->group);

    GPropertyAction *action = g_property_action_new (
        "config.local-echo", self->serial_port, "local-echo");
    g_action_map_add_action (G_ACTION_MAP (self->group), G_ACTION (action));

    action = g_property_action_new ("config.crlf", self->serial_port, "crlf");
    g_action_map_add_action (G_ACTION_MAP (self->group), G_ACTION (action));

    // Work-around to copy the local-echo setting into the global config
    g_signal_connect (G_OBJECT (self->serial_port),
                      "notify::local-echo",
                      G_CALLBACK (on_local_echo_changed),
                      self);

    g_signal_connect (G_OBJECT (self->serial_port),
                      "notify::crlf",
                      G_CALLBACK (on_crlf_changed),
                      self);

    self->display = gt_serial_view_new (self->buffer);

    g_signal_connect_after (
        G_OBJECT (self->display), "commit", G_CALLBACK (on_vte_commit), self);
    g_signal_connect_swapped (G_OBJECT (self->display),
                              "updated",
                              G_CALLBACK (on_display_updated),
                              self);

    gtk_scrolled_window_set_vadjustment (
        GTK_SCROLLED_WINDOW (self->scrolled_window),
        gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self->display)));
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (self->scrolled_window),
                                   self->display);

    self->id = gtk_statusbar_get_context_id (GTK_STATUSBAR (self->status_bar),
                                             "Messages");
    gt_main_window_set_title (self, "Sellerie");

    g_signal_connect (G_OBJECT (self->serial_port),
                      "notify::status",
                      G_CALLBACK (on_serial_port_status_changed),
                      self);

    g_signal_connect (G_OBJECT (self->serial_port),
                      "notify::control",
                      G_CALLBACK (on_serial_port_signals_changed),
                      self);

    g_signal_connect_swapped (G_OBJECT (self->serial_port),
                              "data-available",
                              G_CALLBACK (on_serial_port_data_available),
                              self);

    /* Set up serial signal indicators */
    for (int i = 0; i < SIGNAL_COUNT; i++) {
        GtkWidget *label = gtk_label_new (signal_names[i]);
        gtk_box_append (GTK_BOX (self->status_box), label);
        gtk_widget_set_sensitive (GTK_WIDGET (label), FALSE);
        gtk_widget_set_halign (GTK_WIDGET (label), GTK_ALIGN_END);
        self->signals[i] = label;
    }

    action = g_property_action_new (
        "statusbar-visibility", self->status_box, "visible");
    g_action_map_add_action (G_ACTION_MAP (self->group), G_ACTION (action));

    // Set up hex entry
    gtk_widget_set_visible (GTK_WIDGET (self->hex_box), FALSE);
    g_signal_connect (self->hex_send_entry,
                      "activate",
                      G_CALLBACK (on_send_hexadecimal),
                      self);

    action = g_property_action_new ("view.send-hex", self->hex_box, "visible");
    g_action_map_add_action (G_ACTION_MAP (self->group), G_ACTION (action));

    // VTE popup menu
    GtkGesture *click = gtk_gesture_click_new ();
    gtk_event_controller_set_name (GTK_EVENT_CONTROLLER (click),
                                   "terminal-context-menu");
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), 3);
    gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (click), TRUE);
    gtk_widget_add_controller (GTK_WIDGET (self->display),
                               GTK_EVENT_CONTROLLER (click));

    self->popup_menu = gtk_popover_menu_new_from_model (self->popup_menu_model);
    gtk_widget_set_parent (self->popup_menu, GTK_WIDGET (self));
    gtk_popover_set_position (GTK_POPOVER (self->popup_menu), GTK_POS_BOTTOM);
    gtk_popover_set_has_arrow (GTK_POPOVER (self->popup_menu), FALSE);
    gtk_widget_set_halign (self->popup_menu, GTK_ALIGN_START);

    g_signal_connect (click, "pressed", G_CALLBACK (on_vte_button_press_callback), self);

    g_signal_connect (G_OBJECT (self->display),
                      "selection-changed",
                      G_CALLBACK (on_selection_changed),
                      self);

    on_selection_changed (VTE_TERMINAL (self->display), self);

    action = g_property_action_new ("menubar-visibility", self, "show-menubar");
    g_action_map_add_action (G_ACTION_MAP (self->group), G_ACTION (action));

    g_object_bind_property (
        G_OBJECT (self->logger),
        "active",
        g_action_map_lookup_action (G_ACTION_MAP (self->group), "log.to-file"),
        "enabled",
        G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE);
    g_object_bind_property (G_OBJECT (self->logger),
                            "active",
                            g_action_map_lookup_action (
                                G_ACTION_MAP (self->group), "log.pause-resume"),
                            "enabled",
                            G_BINDING_SYNC_CREATE);
    g_object_bind_property (
        G_OBJECT (self->logger),
        "active",
        g_action_map_lookup_action (G_ACTION_MAP (self->group), "log.stop"),
        "enabled",
        G_BINDING_SYNC_CREATE);
    g_object_bind_property (
        G_OBJECT (self->logger),
        "active",
        g_action_map_lookup_action (G_ACTION_MAP (self->group), "log.clear"),
        "enabled",
        G_BINDING_SYNC_CREATE);

    self->shortcuts = gtk_shortcut_controller_new ();
    gtk_shortcut_controller_set_scope (
        GTK_SHORTCUT_CONTROLLER (self->shortcuts), GTK_SHORTCUT_SCOPE_GLOBAL);

    gt_main_window_set_view (self, GT_MAIN_WINDOW_VIEW_TYPE_ASCII);
}

void
gt_main_window_set_status (GtMainWindow *self, const char *msg)
{
    gt_main_window_pop_status (self);
    gt_main_window_push_status (self, msg);
}

void
gt_main_window_push_status (GtMainWindow *self, const char *msg)
{
    gtk_statusbar_push (GTK_STATUSBAR (self->status_bar), self->id, msg);
}

void
gt_main_window_pop_status (GtMainWindow *self)
{
    gtk_statusbar_pop (GTK_STATUSBAR (self->status_bar), self->id);
}

static gboolean
pop_message_timeout (gpointer user_data)
{
    gt_main_window_pop_status (GT_MAIN_WINDOW (user_data));

    return FALSE;
}

void
gt_main_window_temp_message (GtMainWindow *self, const char *msg, gint timeout)
{
    gtk_statusbar_push (GTK_STATUSBAR (self->status_bar), self->id, msg);
    g_timeout_add (timeout, pop_message_timeout, self);
}

void
gt_main_window_set_title (GtMainWindow *self, const char *msg)
{
    gchar *header = g_strdup_printf ("Sellerie - %s", msg);
    gtk_window_set_title (GTK_WINDOW (self), header);
    g_free (header);
}

void
gt_main_window_set_info_bar (GtMainWindow *self, GtkWidget *widget)
{
    gtk_widget_show (widget);
    gtk_widget_show (self->revealer);
    gtk_revealer_set_child (GTK_REVEALER (self->revealer), widget);
    gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), TRUE);
}

void
gt_main_window_remove_info_bar (GtMainWindow *self, GtkWidget *widget)
{
    gtk_widget_hide (self->revealer);
    gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), FALSE);
    gtk_revealer_set_child (GTK_REVEALER (self->revealer), NULL);
}

GtkWidget *
gt_main_window_get_info_bar (GtMainWindow *self)
{
    return gtk_revealer_get_child (GTK_REVEALER (self->revealer));
}

void
gt_main_window_show_message (GtMainWindow *self,
                             const gchar *message,
                             GtMessageType type)
{
    g_return_if_fail (message != NULL);
    g_return_if_fail (type == GT_MESSAGE_TYPE_ERROR ||
                      type == GT_MESSAGE_TYPE_WARNING);

    GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (self),
                                                GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                                (GtkMessageType)type,
                                                GTK_BUTTONS_OK,
                                                message,
                                                NULL);

    gtk_widget_show (dialog);
    g_signal_connect (
        dialog, "response", G_CALLBACK (gtk_window_destroy), dialog);
}

void
gt_main_window_set_view (GtMainWindow *self, GtMainWindowViewType type)
{
    GActionGroup *group = self->group;
    GAction *show_index =
        g_action_map_lookup_action (G_ACTION_MAP (group), "view.index");
    GAction *hex_width =
        g_action_map_lookup_action (G_ACTION_MAP (group), "view.hex-width");

    switch (type) {
    case GT_MAIN_WINDOW_VIEW_TYPE_ASCII:
        g_simple_action_set_enabled (G_SIMPLE_ACTION (show_index), FALSE);
        g_simple_action_set_enabled (G_SIMPLE_ACTION (hex_width), FALSE);
        gt_serial_view_set_display_mode (GT_SERIAL_VIEW (self->display),
                                         GT_SERIAL_VIEW_TEXT);
        break;
    case GT_MAIN_WINDOW_VIEW_TYPE_HEX:
        g_simple_action_set_enabled (G_SIMPLE_ACTION (show_index), TRUE);
        g_simple_action_set_enabled (G_SIMPLE_ACTION (hex_width), TRUE);
        gt_serial_view_set_display_mode (GT_SERIAL_VIEW (self->display),
                                         GT_SERIAL_VIEW_HEX);
        break;
    default:
        g_assert_not_reached ();
    }
}

void
gt_main_window_clear_display (GtMainWindow *self)
{
    gt_serial_view_clear (GT_SERIAL_VIEW (self->display));
}

static void
on_serial_port_data_available (GtMainWindow *self,
                               GBytes *bytes,
                               gpointer user_data)
{
    gt_buffer_put_bytes (self->buffer, bytes, config.crlfauto);
}

void
on_serial_port_status_changed (GObject *object,
                               GParamSpec *pspec,
                               gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);

    GError *error = gt_serial_port_get_last_error (GT_SERIAL_PORT (object));
    GtSerialPortState status =
        gt_serial_port_get_status (GT_SERIAL_PORT (object));

    if (status == GT_SERIAL_PORT_STATE_ERROR) {
        g_autofree char *msg = NULL;
        if (error != NULL) {
            msg = g_strdup_printf (_ ("Serial port went to error: %s"),
                                   error->message);
        } else {
            msg = g_strdup (_ ("Serial port went to error. Reason unknown."));
        }

        g_warning ("%s", msg);
        GtkWidget *dialog =
            gtk_message_dialog_new (GTK_WINDOW (user_data),
                                    GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_CLOSE,
                                    "%s",
                                    msg);
        gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                      GTK_WINDOW (user_data));
        gtk_widget_show (dialog);
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
        g_signal_connect (
            dialog, "response", G_CALLBACK (gtk_window_destroy), NULL);

    } else if (status == GT_SERIAL_PORT_STATE_OFFLINE) {
        g_debug ("Serial port went offline");
    } else if (status == GT_SERIAL_PORT_STATE_ONLINE) {
        g_debug ("Serial port online");
    }

    char *message = gt_serial_port_to_string (self->serial_port);
    gt_main_window_set_status (self, message);
    gt_main_window_set_title (self, message);
    g_free (message);
}

static void
on_serial_port_signals_changed (GObject *object,
                                GParamSpec *pspec,
                                gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);
    guint port_signals = 0;
    int i = 0;

    port_signals = gt_serial_port_get_signals (GT_SERIAL_PORT (object));

    for (i = 0; i < SIGNAL_COUNT; i++) {
        gboolean active = (port_signals & signal_flags[i]) != 0;
        gtk_widget_set_sensitive (self->signals[i], active);
    }
}

static void
on_send_hexadecimal (GtkWidget *widget, gpointer pointer)
{
    GtMainWindow *self = GT_MAIN_WINDOW (pointer);

    guint i;
    gchar *message = NULL, **tokens = NULL, *buff = NULL;
    guint scan_val;

    const char *text = gtk_editable_get_text (GTK_EDITABLE (widget));

    if (strlen (text) == 0) {
        message = g_strdup_printf (_ ("Nothing sent."));
        gt_main_window_temp_message (self, message, 1500);
        gtk_editable_set_text (GTK_EDITABLE (widget), "");

        goto exit;
    }

    tokens = g_strsplit_set (text, " ;", -1);
    buff = g_malloc0 (g_strv_length (tokens));

    for (i = 0; tokens[i] != NULL; i++) {
        if (sscanf (tokens[i], "%02X", &scan_val) != 1) {
            gt_main_window_temp_message (
                self, _ ("Improper formatted hex input, 0 bytes sent!"), 1500);

            goto exit;
        }
        buff[i] = scan_val;
    }

    gt_serial_port_send_chars (self->serial_port, buff, i);

    message =
        g_strdup_printf (ngettext ("%d byte sent.", "%d bytes sent.", i), i);
    gt_main_window_temp_message (self, message, 2000);
    gtk_editable_set_text (GTK_EDITABLE (widget), "");

exit:
    g_clear_pointer (&message, g_free);
    g_clear_pointer (&tokens, g_strfreev);
    g_clear_pointer (&buff, g_free);
}

static void
on_vte_commit (VteTerminal *widget, gchar *text, guint length, gpointer ptr)
{
    GtMainWindow *self = GT_MAIN_WINDOW (ptr);

    int bytes_written =
        gt_serial_port_send_chars (self->serial_port, text, length);

    if (bytes_written > 0 && config.echo) {
        gt_buffer_put_chars (
            self->buffer, text, bytes_written, config.crlfauto);
    }
}


static void
on_vte_button_press_callback (GtkGesture *click,
          int n_press,
          gdouble x,
          gdouble y,
          gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);
    GdkEventSequence *sequence = gtk_gesture_single_get_current_sequence (
        GTK_GESTURE_SINGLE (click));

    GdkEvent *event = gtk_gesture_get_last_event (click, sequence);

    if (n_press != 1) {
        return;
    }

    if (!gdk_event_triggers_context_menu (event)) {
        return;
    }

    gtk_gesture_set_sequence_state (GTK_GESTURE (click), sequence,
                                    GTK_EVENT_SEQUENCE_CLAIMED);

    GdkRectangle rect = { x, y, 1, 1 };
    gtk_popover_set_pointing_to (GTK_POPOVER (self->popup_menu), &rect);
    gtk_popover_popup (GTK_POPOVER (self->popup_menu));
}

void
on_action_about (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    const gchar *authors[] = {
        "Julien Schimtt", "Zach Davis", "Jens Georg", NULL};
    g_autoptr(GdkTexture) logo = NULL;

    logo = gdk_texture_new_from_resource (
        "/org/jensge/Sellerie/org.jensge.Sellerie.svg");

    gtk_show_about_dialog (GTK_WINDOW (user_data),
                           "program-name",
                           "Sellerie",
                           "version",
                           VERSION,
                           "comments",
                           _ ("Sellerie is a simple GTK+ terminal used to "
                              "communicate with the serial port."),
                           "copyright",
                           "Copyright © Julien Schmitt",
                           "authors",
                           authors,
                           "website",
                           "https://github.com/phako/gtkterm",
                           "website-label",
                           "https://github.com/phako/gtkterm",
                           "license-type",
                           GTK_LICENSE_GPL_3_0,
                           "logo",
                           logo,
                           NULL);
}

void
on_signals_send_break (GSimpleAction *action,
                       GVariant *parameter,
                       gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);

    gt_serial_port_send_brk (self->serial_port);
    gt_main_window_temp_message (self, _ ("Break signal sent!"), 800);
}

void
on_signals_send_dtr (GSimpleAction *action,
                     GVariant *parameter,
                     gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);

    gt_serial_port_set_signals (self->serial_port, 0);
}

void
on_signals_send_rts (GSimpleAction *action,
                     GVariant *parameter,
                     gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);

    gt_serial_port_set_signals (self->serial_port, 1);
}

void
on_quit (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    gtk_window_close (GTK_WINDOW (user_data));
}

void
on_reconnect (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    gt_serial_port_reconnect (GT_MAIN_WINDOW (user_data)->serial_port);
}

void
on_macro (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);

    const char *id = g_variant_get_string (parameter, NULL);
    g_autoptr (GtMacro) macro =
        gt_macro_manager_get (gt_macro_manager_get_default (), id);
    const char *shortcut = gt_macro_get_shortcut (macro);
    GBytes *bytes = gt_macro_get_bytes (macro);
    gsize size;
    const guint8 *data = g_bytes_get_data (bytes, &size);

    g_autofree char *str =
        g_strdup_printf (_ ("Macro \"%s\" sent !"), shortcut);
    gt_main_window_temp_message (self, str, 800);

    on_vte_commit (NULL, (char *)data, size, self);
}

void
on_edit_copy_callback (GSimpleAction *action,
                       GVariant *parameter,
                       gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);
#if VTE_CHECK_VERSION(0, 50, 0)
    vte_terminal_copy_clipboard_format (VTE_TERMINAL (self->display),
                                        VTE_FORMAT_TEXT);
#else
    vte_terminal_copy_clipboard (VTE_TERMINAL (self->display));
#endif
}

void
on_edit_paste_callback (GSimpleAction *action,
                        GVariant *parameter,
                        gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);

    vte_terminal_paste_clipboard (VTE_TERMINAL (self->display));
}

void
on_edit_select_all_callback (GSimpleAction *action,
                             GVariant *parameter,
                             gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);

    vte_terminal_select_all (VTE_TERMINAL (self->display));
}

void
on_selection_changed (VteTerminal *terminal, gpointer data)
{
    GActionGroup *group = NULL;
    GAction *action = NULL;
    gboolean can_copy = FALSE;

    can_copy = vte_terminal_get_has_selection (terminal);

    group = GT_MAIN_WINDOW (data)->group;
    action = g_action_map_lookup_action (G_ACTION_MAP (group), "copy");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_copy);
}

void
on_local_echo_changed (GObject *gobject, GParamSpec *pspec, gpointer user_data)
{
    config.echo =
        gt_serial_port_get_local_echo (GT_MAIN_WINDOW (user_data)->serial_port);
}

void
on_crlf_changed (GObject *gobject, GParamSpec *pspec, gpointer user_data)
{
    config.crlfauto =
        gt_serial_port_get_crlfauto (GT_MAIN_WINDOW (user_data)->serial_port);
}

static void
on_logging_start_response (GtkDialog *self, gint response_id, gpointer data)
{
    GtMainWindow *w = GT_MAIN_WINDOW (data);

    gtk_widget_hide (GTK_WIDGET (self));
    if (response_id == GTK_RESPONSE_OK) {
        GError *error = NULL;
        g_autoptr (GFile) file =
            gtk_file_chooser_get_file (GTK_FILE_CHOOSER (self));
        g_autofree char *file_name = g_file_get_path (file);
        gt_logging_start (w->logger, file_name, &error);
        if (error != NULL) {
            gt_main_window_show_message (
                w, error->message, GT_MESSAGE_TYPE_ERROR);
            g_error_free (error);
        }
    }
    gtk_window_destroy (GTK_WINDOW (self));
}

void
on_logging_start (GSimpleAction *action,
                  GVariant *parameter,
                  gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);

    GtkWidget *file_select;

    file_select = gtk_file_chooser_dialog_new (_ ("Log file selection"),
                                               GTK_WINDOW (self),
                                               GTK_FILE_CHOOSER_ACTION_SAVE,
                                               _ ("_Cancel"),
                                               GTK_RESPONSE_CANCEL,
                                               _ ("_OK"),
                                               GTK_RESPONSE_OK,
                                               NULL);
    const char *default_file = gt_logging_get_default_file (self->logger);
    if (default_file != NULL) {
        g_autoptr (GFile) file = g_file_new_for_commandline_arg (default_file);
        gtk_file_chooser_set_file (GTK_FILE_CHOOSER (file_select), file, NULL);
    }

    gtk_window_set_modal (GTK_WINDOW (file_select), TRUE);
    g_signal_connect (
        file_select, "response", G_CALLBACK (on_logging_start_response), self);
    gtk_widget_show (file_select);
}

void
on_logging_pause_resume (GSimpleAction *action,
                         GVariant *parameter,
                         gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);

    gt_logging_pause_resume (self->logger);
    g_simple_action_set_state (action, parameter);
}

void
on_logging_stop (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);

    gt_logging_stop (self->logger);
}

void
on_logging_clear (GSimpleAction *action,
                  GVariant *parameter,
                  gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);
    GError *error = NULL;

    gt_logging_clear (self->logger, &error);
    if (error != NULL) {
        gt_main_window_show_message (
            self, error->message, GT_MESSAGE_TYPE_ERROR);
        g_error_free (error);
    }
}

void
on_config_port (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    Config_Port_Fenetre (GTK_WINDOW (user_data));
}

void
on_clear_buffer (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);

    gt_buffer_clear (self->buffer);
    gt_main_window_clear_display (self);
}

void
on_view_index_change_state (GSimpleAction *action,
                            GVariant *parameter,
                            gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);

    gt_serial_view_set_show_index (GT_SERIAL_VIEW (self->display),
                                   g_variant_get_boolean (parameter));
    gt_main_window_set_view (self, GT_MAIN_WINDOW_VIEW_TYPE_HEX);
    g_simple_action_set_state (action, parameter);
}

void
on_action_radio (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    g_action_change_state (G_ACTION (action), parameter);
}

void
on_view_ascii_hex_change_state (GSimpleAction *action,
                                GVariant *parameter,
                                gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);

    gsize length = 0;
    const char *value = g_variant_get_string (parameter, &length);
    if (strncmp (value, "hex", length) == 0) {
        gt_main_window_set_view (self, GT_MAIN_WINDOW_VIEW_TYPE_HEX);
    } else if (strncmp (value, "ascii", length) == 0) {
        gt_main_window_set_view (self, GT_MAIN_WINDOW_VIEW_TYPE_ASCII);
    }

    g_simple_action_set_state (action, parameter);
}

void
on_view_hex_width_change_state (GSimpleAction *action,
                                GVariant *parameter,
                                gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);
    gsize length = 0;
    const char *value = g_variant_get_string (parameter, &length);
    gint current_value = atoi (value);

    gt_serial_view_set_bytes_per_line (GT_SERIAL_VIEW (self->display),
                                       current_value);
    gt_main_window_set_view (self, GT_MAIN_WINDOW_VIEW_TYPE_HEX);

    g_simple_action_set_state (action, parameter);
}

static void
on_infobar_close (GtkInfoBar *bar, gpointer user_data)
{
    g_cancellable_cancel (G_CANCELLABLE (user_data));
}

static void
on_infobar_response (GtkInfoBar *bar, gint response_id, gpointer user_data)
{
    g_cancellable_cancel (G_CANCELLABLE (user_data));
}

void
on_file_transfer_ready (GObject *source_object,
                        GAsyncResult *res,
                        gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);
    GError *error = NULL;

    g_debug ("File transfer finished...");
    gt_file_transfer_finish (GT_FILE_TRANSFER (source_object), res, &error);

    GtkWidget *infobar = gt_main_window_get_info_bar (self);
    gt_main_window_remove_info_bar (self, infobar);
    // gtk_widget_destroy (infobar);

    if (error != NULL) {

        if (error->code != G_IO_ERROR_CANCELLED) {
            char *msg = g_strdup_printf ("Failed to send data to port: %s",
                                         error->message);
            g_warning ("%s", msg);
            gt_main_window_show_message (
                GT_MAIN_WINDOW (self), msg, GT_MESSAGE_TYPE_ERROR);
            g_free (msg);

        } else {
            g_debug ("File transfer was cancelled");
        }

        g_error_free (error);
    }

    g_object_unref (source_object);
}

static void
on_send_raw_file_response (GtkDialog *d, gint response_id, gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);
    gtk_widget_hide (GTK_WIDGET (d));

    if (response_id == GTK_RESPONSE_ACCEPT) {
        g_autoptr (GFile) file =
            gtk_file_chooser_get_file (GTK_FILE_CHOOSER (d));
        GtFileTransfer *transfer =
            gt_serial_port_send_file (self->serial_port, file);
        GtkWidget *infobar = gt_infobar_new ();
        g_autofree char *path = g_file_get_path (file);
        g_autofree char *message =
            g_strdup_printf (_ ("Sending file “%s”…"), path);
        gt_infobar_set_label (GT_INFOBAR (infobar), message);
        gt_main_window_set_info_bar (self, infobar);
        g_object_bind_property (G_OBJECT (transfer),
                                "progress",
                                G_OBJECT (infobar),
                                "progress",
                                (GBindingFlags)0);

        GCancellable *cancellable = g_cancellable_new ();
        g_signal_connect (G_OBJECT (infobar),
                          "close",
                          G_CALLBACK (on_infobar_close),
                          cancellable);
        g_signal_connect (G_OBJECT (infobar),
                          "response",
                          G_CALLBACK (on_infobar_response),
                          cancellable);

        gt_file_transfer_start (
            transfer, cancellable, on_file_transfer_ready, self);
    }

    gtk_window_destroy (GTK_WINDOW (d));
}

void
on_send_raw_file (GSimpleAction *action,
                  GVariant *parameter,
                  gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);

    GtkWidget *file_selector =
        gtk_file_chooser_dialog_new (_ ("Send RAW file"),
                                     GTK_WINDOW (self),
                                     GTK_FILE_CHOOSER_ACTION_OPEN,
                                     _ ("_Cancel"),
                                     GTK_RESPONSE_CANCEL,
                                     _ ("_Send"),
                                     GTK_RESPONSE_ACCEPT,
                                     NULL);

    if (self->default_raw_file != NULL) {
        g_autoptr (GFile) file =
            g_file_new_for_commandline_arg (self->default_raw_file);
        gtk_file_chooser_set_file (
            GTK_FILE_CHOOSER (file_selector), file, NULL);
    }

    gtk_dialog_set_default_response (GTK_DIALOG (file_selector),
                                     GTK_RESPONSE_ACCEPT);
    gtk_window_set_transient_for (GTK_WINDOW (file_selector),
                                  GTK_WINDOW (self));
    gtk_window_set_modal (GTK_WINDOW (file_selector), TRUE);

    g_signal_connect (file_selector,
                      "response",
                      G_CALLBACK (on_send_raw_file_response),
                      self);

    gtk_widget_show (file_selector);
}

static void
on_save_raw_file_response (GtkDialog *file_select, gint result, gpointer data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (data);

    gtk_widget_hide (GTK_WIDGET (file_select));

    if (result == GTK_RESPONSE_ACCEPT) {
        g_autoptr (GFile) file =
            gtk_file_chooser_get_file (GTK_FILE_CHOOSER (file_select));
        if (file == NULL) {
            g_autofree char *msg = g_strdup_printf (_ ("File error\n"));
            gt_main_window_show_message (self, msg, GT_MESSAGE_TYPE_ERROR);
        } else {
            g_autofree char *fileName = g_file_get_path (file);

            GError *error = NULL;
            gt_buffer_write_to_file (self->buffer, fileName, &error);

            if (error != NULL) {
                g_autofree char *msg = g_strdup_printf (
                    _ ("Failed to write buffer to file %s: %s"),
                    fileName,
                    error->message);
                gt_main_window_show_message (self, msg, GT_MESSAGE_TYPE_ERROR);
                g_error_free (error);
            }
        }
    }
    gtk_window_destroy (GTK_WINDOW (file_select));
}

void
on_save_raw_file (GSimpleAction *action,
                  GVariant *parameter,
                  gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);

    GtkWidget *file_select =
        gtk_file_chooser_dialog_new (_ ("Save RAW File"),
                                     GTK_WINDOW (self),
                                     GTK_FILE_CHOOSER_ACTION_SAVE,
                                     _ ("_Cancel"),
                                     GTK_RESPONSE_CANCEL,
                                     _ ("_OK"),
                                     GTK_RESPONSE_ACCEPT,
                                     NULL);
    gtk_window_set_modal (GTK_WINDOW (file_select), TRUE);
    g_signal_connect (
        file_select, "response", G_CALLBACK (on_save_raw_file_response), self);
    gtk_widget_show (file_select);
}

static void
on_config_terminal_response (GtkDialog *d, gint response_id, gpointer data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (data);
    gtk_widget_hide (GTK_WIDGET (d));
    gtk_window_destroy (GTK_WINDOW (d));

    PangoFontDescription *font_desc = NULL;
    GdkRGBA *text = NULL;
    GdkRGBA *background = NULL;
    guint scrollback_lines = 0;

    g_object_get (self->display,
                  "font-desc",
                  &font_desc,
                  "text",
                  &text,
                  "background",
                  &background,
                  "scrollback-lines",
                  &scrollback_lines,
                  NULL);
    gt_config_set_view_config (font_desc, text, background, scrollback_lines);
}

void
on_config_terminal (GSimpleAction *action,
                    GVariant *parameter,
                    gpointer user_data)
{
    GtMainWindow *self = GT_MAIN_WINDOW (user_data);
    GtkWidget *d = gt_view_config_new (GT_SERIAL_VIEW (self->display));
    gtk_window_set_transient_for (GTK_WINDOW (d), GTK_WINDOW (self));
    gtk_window_set_modal (GTK_WINDOW (d), TRUE);
    gtk_widget_show (d);
    g_signal_connect (
        d, "response", G_CALLBACK (on_config_terminal_response), self);
}

void
on_config_macros (GSimpleAction *action,
                  GVariant *parameter,
                  gpointer user_data)
{
    GtkWidget *dialog = gt_macro_editor_new (gt_macro_manager_get_default());
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (user_data));
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
    gtk_widget_show (dialog);
    g_signal_connect (
        dialog, "response", G_CALLBACK (gtk_window_destroy), NULL);
}

void
on_config_profile_select (GSimpleAction *action,
                          GVariant *parameter,
                          gpointer user_data)
{
    select_config_callback (NULL);
}

void
on_config_profile_save (GSimpleAction *action,
                        GVariant *parameter,
                        gpointer user_data)
{
    save_config_callback (NULL);
}

void
on_config_profile_delete (GSimpleAction *action,
                          GVariant *parameter,
                          gpointer user_data)
{
    delete_config_callback (NULL);
}

static void
on_display_updated (GtMainWindow *self,
                    gchar *text,
                    guint length,
                    gpointer user_data)
{
    GError *error = NULL;

    gt_logging_log (self->logger, text, length, &error);
    if (error != NULL) {
        gt_main_window_show_message (
            self, error->message, GT_MESSAGE_TYPE_ERROR);
        g_error_free (error);
    }
}
