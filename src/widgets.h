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

#ifndef WIDGETS_H_
#define WIDGETS_H_

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#define MSG_WRN 0
#define MSG_ERR 1

#define ASCII_VIEW 0
#define HEXADECIMAL_VIEW 1

void create_main_window(GtkApplication *app);

void gt_main_window_set_status (const char *msg);
void gt_main_window_push_status (const char *msg);
void gt_main_window_pop_status (void);
void gt_main_window_add_shortcut (guint key, GdkModifierType mod, GClosure *closure);
void gt_main_window_remove_shortcut (GClosure *closure);

void put_text(gchar *, guint);
void put_hexadecimal(gchar *, guint);
void Set_local_echo(gboolean);
void show_message(const gchar *, gint);
void clear_display(void);
void set_view(guint);
gint send_serial(gchar *, gint);
void Put_temp_message(const gchar *, gint);
void Set_window_title(const gchar *msg);
void Set_crlfauto(gboolean crlfauto);

void toggle_logging_pause_resume(gboolean currentlyLogging);
void toggle_logging_sensitivity(gboolean currentlyLogging);

#endif
