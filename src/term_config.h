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

#ifndef TERM_CONFIG_H_
#define TERM_CONFIG_H_

#include <gtk/gtk.h>

void Config_Port_Fenetre(GtkWindow *parent);
gint Lis_Config(GtkBuilder *builder);
void Config_Terminal(GtkAction *action, gpointer data);
void select_config_callback(GtkAction *action, gpointer data);
void save_config_callback(GtkAction *action, gpointer data);
void delete_config_callback(GtkAction *action, gpointer data);
void Verify_configuration(void);
gint Load_configuration_from_file(const gchar *);
gint Check_configuration_file(void);
void check_text_input(GtkEditable *editable,
		       gchar       *new_text,
		       gint         new_text_length,
		       gint        *position,
		       gpointer     user_data);
void update_vte_config (void);

struct configuration_port {
  gchar port[1024];
  gint vitesse;                // 300 - 600 - 1200 - ... - 115200
  gint bits;                   // 5 - 6 - 7 - 8
  gint stops;                  // 1 - 2
  gint parite;                 // 0 : None, 1 : Odd, 2 : Even
  gint flux;                   // 0 : None, 1 : Xon/Xoff, 2 : RTS/CTS, 3 : RS485halfduplex
  gint delai;                  // end of char delay: in ms
  gint rs485_rts_time_before_transmit;
  gint rs485_rts_time_after_transmit;
  gchar car;             // caractere à attendre
  gboolean echo;               // echo local
  gboolean crlfauto;         // line feed auto
};
typedef struct configuration_port GtSerialPortConfiguration;


#define DEFAULT_FONT "Monospace, 12"
#define DEFAULT_SCROLLBACK 200

#define DEFAULT_PORT "/dev/ttyS0"
#define DEFAULT_SPEED 9600
#define DEFAULT_PARITY 0
#define DEFAULT_BITS 8
#define DEFAULT_STOP 1
#define DEFAULT_FLOW 0
#define DEFAULT_DELAY 0
#define DEFAULT_CHAR -1
#define DEFAULT_DELAY_RS485 30
#define DEFAULT_ECHO FALSE

const char *gt_config_get_file_path (void);
void gt_config_set_file_path (const char *path);

#endif
