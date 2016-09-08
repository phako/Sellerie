/***********************************************************************/
/* buffer.h                                                            */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Management of a local buffer of data received                  */
/*      - Header file -                                                */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 0.99.7 : removed auto crlf stuff - (use macros instead)      */
/*      - 0.98.4 : file creation by Julien                             */
/*                                                                     */
/***********************************************************************/

#ifndef BUFFER_H_
#define BUFFER_H_

#include <glib.h>

void create_buffer(void);
void delete_buffer(void);
void put_chars(char *, unsigned int, gboolean);
void clear_buffer(void);
void write_buffer(void);
void set_display_func(void (*func)(char *, unsigned int));
void unset_display_func(void (*func)(char *, unsigned int));
void set_clear_func(void (*func)(void));
void unset_clear_func(void (*func)(void));
void write_buffer_with_func(void (*func)(char *, unsigned int));

#endif
