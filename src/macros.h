/***********************************************************************/
/* macros.h                                                            */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Functions for the management of the macros                     */
/*      - Header file -                                                */
/*                                                                     */
/***********************************************************************/

#ifndef MACROS_H_
#define MACROS_H_

#include <gtk/gtk.h>

typedef struct
{
  gchar *shortcut;
  gchar *action;
  GClosure *closure;
}
macro_t;

void Config_macros(GtkWindow *parent);
void remove_shortcuts(void);
void add_shortcuts(void);
void create_shortcuts(macro_t *, gint);
macro_t *get_shortcuts(gint *);

#endif
