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

#ifndef GT_MAIN_WINDOW_H
#define GT_MAIN_WINDOW_H

#include "serial-port.h"
#include "logging.h"
#include "buffer.h"

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SIGNAL_COUNT 6

struct _GtMainWindow
{
    GtkApplicationWindow parent_instance;
    GtkWidget *display;
    GtkBox *main_box;
    GtkWidget *hex_box;
    GtkWidget *hex_send_entry;
    GtkWidget *status_bar;
    GtkWidget *scrolled_window;
    guint id;
    GtSerialPort *serial_port;
    GtBuffer *buffer;
    GtkWidget *signals[SIGNAL_COUNT];
    GtkWidget *popup_menu;
    GtkWidget *revealer;
    GtLogging *logger;
    GActionGroup *group;
    GtkEventController *shortcuts;
    char *default_raw_file;
};

enum _GtMessageType {
    GT_MESSAGE_TYPE_ERROR = GTK_MESSAGE_ERROR,
    GT_MESSAGE_TYPE_WARNING = GTK_MESSAGE_WARNING
};

typedef enum _GtMessageType GtMessageType;

#define GT_TYPE_MAIN_WINDOW (gt_main_window_get_type())

G_DECLARE_FINAL_TYPE (GtMainWindow, gt_main_window, GT, MAIN_WINDOW, GtkApplicationWindow)

GtkWidget *gt_main_window_new (GtkApplication *app);
void gt_main_window_set_status (GtMainWindow *self, const char *msg);
void gt_main_window_push_status (GtMainWindow *self, const char *msg);
void gt_main_window_pop_status (GtMainWindow *self);
void gt_main_window_temp_message (GtMainWindow *self,
                                  const char *msg,
                                  gint timeout);
void gt_main_window_set_title (GtMainWindow *self,
                               const char   *msg);

void gt_main_window_remove_info_bar (GtMainWindow *self, GtkWidget *widget);
void
gt_main_window_set_info_bar (GtMainWindow *self, GtkWidget *widget);
GtkWidget *gt_main_window_get_info_bar (GtMainWindow *self);
void gt_main_window_show_message (GtMainWindow *self, const char *message, GtMessageType type);
void gt_main_window_add_shortcut (GtMainWindow *self, guint key, GdkModifierType mod, GClosure *closure);
void gt_main_window_remove_shortcut (GtMainWindow *self, GClosure *closure);

G_END_DECLS

#endif /* GT_MAIN_WINDOW_H */
