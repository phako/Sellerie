/***********************************************************************/
/* buffer.c                                                            */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Management of a local buffer of data received                  */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 0.99.7 : removed (send)auto crlf stuff - (use macros instead)*/
/*      - 0.99.5 : Corrected segfault in case of buffer overlap        */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.98.4 : file creation by Julien                             */
/*                                                                     */
/***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "buffer.h"
#include "i18n.h"

/* For BUFFER_RECEPTION */
#include "serial-port.h"

#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>
#include <glib.h>

#define BUFFER_SIZE (128 * 1024)

typedef struct {
    char *buffer;
    gboolean cr_received;
    unsigned int pointer;
    char *current_buffer;
    gboolean overlapped;
    GtBufferFunc write_func;
} GtBufferPrivate;

struct _GtBuffer {
    GObject parent_instance;
};

struct _GtBufferClass {
    GObjectClass parent_class;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtBuffer, gt_buffer, G_TYPE_OBJECT)

/* GObject overrides */
static void
gt_buffer_finalize (GObject *object);

static void
gt_buffer_class_init (GtBufferClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gt_buffer_finalize;
}

static void
gt_buffer_init (GtBuffer *self)
{
    GtBufferPrivate *priv = gt_buffer_get_instance_private (self);
    priv->buffer = g_malloc0 (BUFFER_SIZE);
    priv->current_buffer = priv->buffer;
}

static void
gt_buffer_finalize (GObject *object)
{
    GtBuffer *self = GT_BUFFER (object);
    GtBufferPrivate *priv = gt_buffer_get_instance_private (self);

    g_clear_pointer (&priv->buffer, g_free);
}

GtBuffer *
gt_buffer_new (void)
{
    return GT_BUFFER (g_object_new (GT_TYPE_BUFFER, NULL));
}

void
gt_buffer_put_chars (GtBuffer *self,
                     char *chars,
                     unsigned int size,
                     gboolean crlf_auto)
{
    GtBufferPrivate *priv = gt_buffer_get_instance_private (self);
    char *characters = NULL;

    g_return_if_fail (self != NULL);
 
    /* If the auto CR LF mode on, read the buffer to add \r before \n */ 
    if (crlf_auto)
    {
        /* BUFFER_RECEPTION*2 for worst case scenario, all \n or \r chars */
        char out_buffer[BUFFER_RECEPTION*2];
        unsigned int i, out_size = 0;

        for (i = 0; i < size; i++)
        {
            if (chars[i] == '\r')
            {
                /* If the previous character was a CR too, insert a newline */
                if (priv->cr_received)
                {
                    out_buffer[out_size] = '\n';
                    out_size++;
                }
                priv->cr_received = TRUE;
            }
            else
            {
                if (chars[i] == '\n')
                {
                    /* If we get a newline without a CR first, insert a CR */
                    if (!priv->cr_received)
                    {
                        out_buffer[out_size] = '\r';
                        out_size++;
                    }
                }
                else
                {
                    /* If we receive a normal char, and the previous one was a
                       CR insert a newline */
                    if (priv->cr_received)
                    {
                        out_buffer[out_size] = '\n';
                        out_size++;
                    }
                }
                priv->cr_received = FALSE;
            }
            out_buffer[out_size] = chars[i];
            out_size++;
        }

        chars = out_buffer;
        size = out_size;
    }

    if (size > BUFFER_SIZE)
    {
        characters = chars + (size - BUFFER_SIZE);
        size = BUFFER_SIZE;
    }
    else
    {
        characters = chars;
    }

    if ((size + priv->pointer) >= BUFFER_SIZE)
    {
        memcpy (priv->current_buffer, characters, BUFFER_SIZE - priv->pointer);
        chars = characters + BUFFER_SIZE - priv->pointer;
        priv->pointer = size - (BUFFER_SIZE - priv->pointer);
        memcpy (priv->buffer, chars, priv->pointer);
        priv->current_buffer = priv->buffer + priv->pointer;
        priv->overlapped = TRUE;
    }
    else
    {
        memcpy (priv->current_buffer, characters, size);
        priv->pointer += size;
        priv->current_buffer += size;
    }

    if (priv->write_func != NULL)
        priv->write_func (characters, size);
}

void
gt_buffer_clear (GtBuffer *self)
{
    GtBufferPrivate *priv = gt_buffer_get_instance_private (self);

    priv->overlapped = FALSE;
    memset (priv->buffer, 0, BUFFER_SIZE);
    priv->current_buffer = priv->buffer;
    priv->pointer = 0;
    priv->cr_received = FALSE;
}

void
gt_buffer_write (GtBuffer *self)
{
    GtBufferPrivate *priv = gt_buffer_get_instance_private (self);

    if (priv->write_func == NULL)
    {
        return;
    }

    /* Write the second half of the ringbuffer first (contains start of data) */
    if (priv->overlapped)
    {
        priv->write_func (priv->current_buffer, BUFFER_SIZE - priv->pointer);
    }
    priv->write_func (priv->buffer, priv->pointer);
}

void
gt_buffer_write_with_func (GtBuffer *self, GtBufferFunc write_func)
{
    GtBufferPrivate *priv = gt_buffer_get_instance_private (self);
    GtBufferFunc old_write_func;

    old_write_func = priv->write_func;
    priv->write_func = write_func;
    gt_buffer_write (self);
    priv->write_func = old_write_func;
}

void
gt_buffer_set_display_func (GtBuffer *self, GtBufferFunc write_func)
{
    GtBufferPrivate *priv = gt_buffer_get_instance_private (self);

    priv->write_func = write_func;
}

void
gt_buffer_unset_display_func (GtBuffer *self)
{
    GtBufferPrivate *priv = gt_buffer_get_instance_private (self);

    priv->write_func = NULL;
}

