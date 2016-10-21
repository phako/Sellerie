/***********************************************************************/
/* config.h                                                            */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Configuration of the serial port                               */
/*      - Header file -                                                */
/*                                                                     */
/***********************************************************************/
#ifndef TERM_CONFIG_H
#define TERM_CONFIG_H

typedef enum {
    GT_SERIAL_PARITY_NONE,
    GT_SERIAL_PARITY_ODD,
    GT_SERIAL_PARITY_EVEN
} GtSerialParity;

typedef enum {
    GT_SERIAL_FLOW_NONE,
    GT_SERIAL_FLOW_XON,
    GT_SERIAL_FLOW_RTS,
    GT_SERIAL_FLOW_RS485
} GtSerialFlow;

#include <gtk/gtk.h>

void Config_Port_Fenetre(GtkWindow *parent);
gint Lis_Config(GtkBuilder *builder);
void Config_Terminal(GtkAction *action, gpointer data);
void select_config_callback(GtkAction *action, gpointer data);
void save_config_callback(GtkAction *action, gpointer data);
void delete_config_callback(GtkAction *action, gpointer data);
gint Check_configuration_file(void);
typedef struct configuration_port GtSerialPortConfiguration;

const char *gt_config_get_file_path (void);
void gt_config_set_file_path (const char *path);
int gt_config_load_profile (const char *);
void gt_config_validate (void);

GtSerialPortConfiguration *gt_config_get (void);

int gt_config_get_delay (GtSerialPortConfiguration *config);
int gt_config_get_wait_char (GtSerialPortConfiguration *config);

void gt_config_set_echo (GtSerialPortConfiguration *config, gboolean echo);
gboolean gt_config_get_echo (GtSerialPortConfiguration *config);

void gt_config_set_auto_crlf (GtSerialPortConfiguration *config, gboolean crlf);
gboolean gt_config_get_auto_crlf (GtSerialPortConfiguration *config);

void gt_config_set_serial_parity (GtSerialPortConfiguration *config, GtSerialParity parity);
GtSerialParity gt_config_get_serial_parity (GtSerialPortConfiguration *config);

void gt_config_set_serial_flow (GtSerialPortConfiguration *config, GtSerialFlow flow);
GtSerialFlow gt_config_get_serial_flow (GtSerialPortConfiguration *config);

void gt_config_set_port (GtSerialPortConfiguration *config, const char *port);
const char *gt_config_get_port (GtSerialPortConfiguration *config);
void gt_config_set_serial_speed (GtSerialPortConfiguration *config, int speed);
int gt_config_get_serial_speed (GtSerialPortConfiguration *config);
int gt_config_get_serial_bits (GtSerialPortConfiguration *config);
int gt_config_get_serial_stop_bits (GtSerialPortConfiguration *);

int gt_config_get_rs485_before_tx_time (GtSerialPortConfiguration *);
int gt_config_get_rs485_after_tx_time (GtSerialPortConfiguration *);
#endif
