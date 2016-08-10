/***********************************************************************/
/* fichier.h                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Raw / text file transfer management                            */
/*      - Header file -                                                */
/*                                                                     */
/***********************************************************************/

#ifndef FICHIER_H_
#define FICHIER_H_

void send_raw_file(GtkAction *action, gpointer data);
void save_raw_file(GtkAction *action, gpointer data);
void add_input(void);

gboolean gt_file_get_waiting_for_char (void);
void gt_file_set_waiting_for_char (gboolean waiting);

const char *gt_file_get_default (void);
void gt_file_set_default (const char *file);


#endif
