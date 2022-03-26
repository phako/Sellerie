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

typedef enum _GtSerialPortFlowControl {
    GT_SERIAL_PORT_FLOW_CONTROL_NONE,
    GT_SERIAL_PORT_FLOW_CONTROL_XON,
    GT_SERIAL_PORT_FLOW_CONTROL_RTS,
    GT_SERIAL_PORT_FLOW_CONTROL_RS485
} GtSerialPortFlowControl;

typedef enum _GtSerialPortParity {
    GT_SERIAL_PORT_PARITY_NONE,
    GT_SERIAL_PORT_PARITY_ODD,
    GT_SERIAL_PORT_PARITY_EVEN,
} GtSerialPortParity;

void Config_Port_Fenetre(GtkWindow *parent);
gint Lis_Config(GtkBuilder *builder);
void
Config_Terminal (gpointer data);
void
select_config_callback (gpointer data);
void
save_config_callback (gpointer data);
void
delete_config_callback (gpointer data);
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
  GtSerialPortParity parity;   // 0 : None, 1 : Odd, 2 : Even
  GtSerialPortFlowControl
      flow; // 0 : None, 1 : Xon/Xoff, 2 : RTS/CTS, 3 : RS485halfduplex
  gint delai;                  // end of char delay: in ms
  gint rs485_rts_time_before_transmit;
  gint rs485_rts_time_after_transmit;
  gchar car;             // caractere à attendre
  gboolean echo;               // echo local
  gboolean crlfauto;         // line feed auto
};
typedef struct configuration_port GtSerialPortConfiguration;


const char *gt_config_get_file_path (void);
void gt_config_set_file_path (const char *path);
void
gt_config_set_view_config (PangoFontDescription *desc,
                           const GdkRGBA *fg,
                           const GdkRGBA *bg,
                           guint lines);

#endif
