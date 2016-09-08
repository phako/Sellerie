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

#include <gtk/gtk.h>
#include <glib.h>

void send_raw_file(GtkWindow *parent);
void save_raw_file(GtkWindow *parent);
void add_input(void);

gboolean gt_file_get_waiting_for_char (void);
void gt_file_set_waiting_for_char (gboolean waiting);

const char *gt_file_get_default (void);
void gt_file_set_default (const char *file);


#endif
