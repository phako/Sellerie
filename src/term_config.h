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

#include <gtk/gtk.h>

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

void Config_Port_Fenetre(GtkWindow *parent);
void Config_Terminal(GtkAction *action, gpointer data);
void select_config_callback(GtkAction *action, gpointer data);
void save_config_callback(GtkAction *action, gpointer data);
void delete_config_callback(GtkAction *action, gpointer data);

void gt_config_load_profile (const char *);
const char *gt_config_get_profile (void);

GSettings *gt_config_get_settings_for_profile (const char *id);
GSettings *gt_config_get_profile_settings (void);
#endif
