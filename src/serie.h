/***********************************************************************/
/* serie.c                                                             */
/* -------                                                             */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Serial port access functions                                   */
/*      - Header file -                                                */
/*                                                                     */
/***********************************************************************/

#ifndef SERIE_H_
#define SERIE_H_

#include <glib.h>

int Send_chars(char *, int);
gboolean Config_port(void);
void Set_signals(guint);
int lis_sig(void);
void Close_port_and_remove_lockfile(void);
void configure_echo(gboolean);
void configure_crlfauto(gboolean);
void sendbreak(void);
gint set_custom_speed(int, int);
gchar* get_port_string(void);
gint gt_serial_port_get_fd (void);


#define BUFFER_RECEPTION 8192
#define BUFFER_EMISSION 4096
#define LINE_FEED 0x0A
#define POLL_DELAY 100               /* in ms (for control signals) */


#endif
