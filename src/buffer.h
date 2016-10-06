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

#ifndef BUFFER_H
#define BUFFER_H

#include <glib.h>

#include <glib-object.h>

G_BEGIN_DECLS

#define GT_TYPE_BUFFER gt_buffer_get_type ()
G_DECLARE_FINAL_TYPE (GtBuffer, gt_buffer, GT, BUFFER, GObject)

typedef struct _GtBuffer GtBuffer;

typedef void (*GtBufferFunc) (char *, unsigned int);
typedef void (*GtBufferClearFunc) (void);

GtBuffer *gt_buffer_new (void);
void gt_buffer_put_chars (GtBuffer *, char *, unsigned int, gboolean);
void gt_buffer_clear (GtBuffer *);
void gt_buffer_write (GtBuffer *);
void gt_buffer_write_with_func (GtBuffer *, GtBufferFunc);
void gt_buffer_set_display_func (GtBuffer *, GtBufferFunc);
void gt_buffer_unset_display_func (GtBuffer *);
void gt_buffer_set_clear_func (GtBuffer *, GtBufferClearFunc);
void gt_buffer_unset_clear_func (GtBuffer *);

G_END_DECLS

#endif
